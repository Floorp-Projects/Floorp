/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function MenuManager (commandManager, menuSpecs, contextFunction, commandStr)
{
    var menuManager = this;

    this.commandManager = commandManager;
    this.menuSpecs = menuSpecs;
    this.contextFunction = contextFunction;
    this.commandStr = commandStr;

    this.onPopupShowing =
        function mmgr_onshow (event) { return menuManager.showPopup (event); };
    this.onPopupHiding =
        function mmgr_onhide (event) { return menuManager.hidePopup (event); };
}

MenuManager.prototype.appendMenuItems =
function mmgr_append(menuId, items)
{
    for (var i = 0; i < items.length; ++i)
        client.menuSpecs[menuId].items.push(items[i]);
}

MenuManager.prototype.createContextMenus =
function mmgr_initcxs (document)
{
    for (var id in this.menuSpecs)
    {
        if (id.indexOf("context:") == 0)
            this.createContextMenu(document, id);
    }
}

MenuManager.prototype.createContextMenu =
function mmgr_initcx (document, id)
{
    if (!document.getElementById(id))
    {
        if (!ASSERT(id in this.menuSpecs, "unknown context menu " + id))
            return;

        var dp = document.getElementById("dynamic-popups");
        var popup = this.appendPopupMenu (dp, null, id, id);
        var items = this.menuSpecs[id].items;
        this.createMenuItems (popup, null, items);
    }
}


MenuManager.prototype.createMenus =
function mmgr_createtb(document, menuid)
{
    var menu = document.getElementById(menuid);
    for (id in this.menuSpecs)
    {
        var domID;
        if ("domID" in this.menuSpecs[id])
            domID = this.menuSpecs[id].domID;
        else
            domID = id;
        
        if (id.indexOf(menuid + ":") == 0)
            this.createMenu(menu, null, id, domID);
    }
}

MenuManager.prototype.createMainToolbar =
function mmgr_createtb(document, id)
{
    var toolbar = document.getElementById(id);
    var spec = client.menuSpecs[id];
    for (var i in spec.items)
    {
        this.appendToolbarItem (toolbar, null, spec.items[i]);
    }

    toolbar.className = "toolbar-primary chromeclass-toolbar";
}


/**
 * Internal use only.
 *
 * Registers event handlers on a given menu.
 */
MenuManager.prototype.hookPopup =
function mmgr_hookpop (node)
{
    node.addEventListener ("popupshowing", this.onPopupShowing, false);
    node.addEventListener ("popuphiding",  this.onPopupHiding, false);
}

/**
 * Internal use only.
 *
 * |showPopup| is called from the "onpopupshowing" event of menus
 * managed by the CommandManager.  If a command is disabled, represents a command
 * that cannot be "satisfied" by the current command context |cx|, or has an
 * "enabledif" attribute that eval()s to false, then the menuitem is disabled.
 * In addition "checkedif" and "visibleif" attributes are eval()d and
 * acted upon accordingly.
 */
MenuManager.prototype.showPopup =
function mmgr_showpop (event)
{
    //dd ("showPopup {");
    /* returns true if the command context has the properties required to
     * execute the command associated with |menuitem|.
     */
    function satisfied()
    {
        if (menuitem.hasAttribute("isSeparator") || 
            !menuitem.hasAttribute("commandname"))
        {
            return true;
        }

        if (!("menuManager" in cx))
        {
            dd ("no menuManager in cx");
            return false;
        }
        
        var name = menuitem.getAttribute("commandname");
        var commandManager = cx.menuManager.commandManager;
        var commands = commandManager.commands;

        if (!ASSERT (name in commands,
                     "menu contains unknown command '" + name + "'"))
        {
            return false;
        }

        var rv = commandManager.isCommandSatisfied(cx, commands[name]);
        delete cx.parseError;
        return rv;
    };
    
    /* Convenience function for "enabledif", etc, attributes. */
    function has (prop)
    {
        return (prop in cx);
    };
    
    /* evals the attribute named |attr| on the node |node|. */
    function evalIfAttribute (node, attr)
    {
        var ex;
        var expr = node.getAttribute(attr);
        if (!expr)
            return true;
        
        expr = expr.replace (/\Wand\W/gi, " && ");
        expr = expr.replace (/\Wor\W/gi, " || ");

        try
        {
            return eval("(" + expr + ")");
        }
        catch (ex)
        {
            dd ("caught exception evaling '" + node.getAttribute("id") + "'.'" +
                attr + "': '" + expr + "'\n" + ex);
        }
        return true;
    };
    
    var cx;
    var popup = event.originalTarget;
    var menuitem = popup.firstChild;

    /* If the host provided a |contextFunction|, use it now.  Remember the
     * return result as this.cx for use if something from this menu is actually
     * dispatched.  this.cx is deleted in |hidePopup|. */
    if (typeof this.contextFunction == "function")
    {
        cx = this.cx = this.contextFunction (popup.getAttribute("menuName"),
                                             event);
    }
    else
    {
        cx = this.cx = { menuManager: this, originalEvent: event };
    }

    do
    {
        /* should it be visible? */
        if (menuitem.hasAttribute("visibleif"))
        {
            if (evalIfAttribute(menuitem, "visibleif"))
                menuitem.removeAttribute ("hidden");
            else
            {
                menuitem.setAttribute ("hidden", "true");
                continue;
            }
        }

        /* it's visible, maybe it has a dynamic label? */
        if (menuitem.hasAttribute("format"))
        {
            var label = replaceVars(menuitem.getAttribute("format"), cx);
            if (label.indexOf("\$") != -1)
                label = menuitem.getAttribute("backupLabel");
            menuitem.setAttribute("label", label);
        }
        
        /* ok, it's visible, maybe it should be disabled? */
        if (satisfied())
        {
            if (menuitem.hasAttribute("enabledif"))
            {
                if (evalIfAttribute(menuitem, "enabledif"))
                    menuitem.removeAttribute ("disabled");
                else
                    menuitem.setAttribute ("disabled", "true");
            }
            else
                menuitem.removeAttribute ("disabled");
        }
        else
        {
            menuitem.setAttribute ("disabled", "true");
        }
        
        /* should it have a check? */
        if (menuitem.hasAttribute("checkedif"))
        {
            if (evalIfAttribute(menuitem, "checkedif"))
                menuitem.setAttribute ("checked", "true");
            else
                menuitem.removeAttribute ("checked");
        }
        
    } while ((menuitem = menuitem.nextSibling));

    //dd ("}");
    
    return true;
}

/**
 * Internal use only.
 *
 * |hidePopup| is called from the "onpopuphiding" event of menus
 * managed by the CommandManager.  Nothing to do here anymore.
 * We used to just clean up this.cx, but that's a problem for nested
 * menus.
 */
MenuManager.prototype.hidePopup =
function mmgr_hidepop (id)
{
    return true;
}

/**
 * Appends a sub-menu to an existing menu.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param id      ID of the sub-menu to add.
 * @param label   Text to use for this sub-menu.  The & character can be
 *                used to indicate the accesskey.
 * @param attribs Object containing CSS attributes to set on the element.
 */
MenuManager.prototype.appendSubMenu =
function mmgr_addsmenu (parentNode, beforeNode, menuName, domId, label, attribs)
{
    var document = parentNode.ownerDocument;
    
    /* sometimes the menu is already there, for overlay purposes. */
    var menu = document.getElementById(domId);
    
    if (!menu)
    {
        menu = document.createElement ("menu");
        menu.setAttribute ("id", domId);
        parentNode.insertBefore(menu, beforeNode);
    }

    var menupopup = menu.firstChild;
    
    if (!menupopup)
    {
        menupopup = document.createElement ("menupopup");
        menupopup.setAttribute ("id", domId + "-popup");
        menu.appendChild(menupopup);    
        menupopup = menu.firstChild;
    }

    menupopup.setAttribute ("menuName", menuName);

    menu.setAttribute ("accesskey", getAccessKey(label));
    label = label.replace("&", "");
    menu.setAttribute ("label", label);
    menu.setAttribute ("isSeparator", true);

    if (typeof attribs == "object")
    {
        for (var p in attribs)
            menu.setAttribute (p, attribs[p]);
    }

    this.hookPopup (menupopup);

    return menupopup;
}

/**
 * Appends a popup to an existing popupset.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param id      ID of the popup to add.
 * @param label   Text to use for this popup.  Popup menus don't normally have
 *                labels, but we set a "label" attribute anyway, in case
 *                the host wants it for some reason.  Any "&" characters will
 *                be stripped.
 * @param attribs Object containing CSS attributes to set on the element.
 */
MenuManager.prototype.appendPopupMenu =
function mmgr_addpmenu (parentNode, beforeNode, menuName, id, label, attribs)
{
    var document = parentNode.ownerDocument;
    var popup = document.createElement ("popup");
    popup.setAttribute ("id", id);
    if (label)
        popup.setAttribute ("label", label.replace("&", ""));
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            popup.setAttribute (p, attribs[p]);
    }

    popup.setAttribute ("menuName", menuName);

    parentNode.insertBefore(popup, beforeNode);
    this.hookPopup (popup);

    return popup;
}

/**
 * Appends a menuitem to an existing menu or popup.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param command A reference to the CommandRecord this menu item will represent.
 * @param attribs Object containing CSS attributes to set on the element.
 */
MenuManager.prototype.appendMenuItem =
function mmgr_addmenu (parentNode, beforeNode, commandName, attribs)
{
    var menuManager = this;    
    
    var document = parentNode.ownerDocument;
    if (commandName == "-")
        return this.appendMenuSeparator(parentNode, beforeNode, attribs);
    
    var parentId = parentNode.getAttribute("id");
    
    if (!ASSERT(commandName in this.commandManager.commands,
                "unknown command " + commandName + " targeted for " +
                parentId))
    {
        return null;
    }

    var command = this.commandManager.commands[commandName];
    var menuitem = document.createElement ("menuitem");
    menuitem.setAttribute ("id", parentId + ":" + commandName);
    menuitem.setAttribute ("commandname", command.name);
    menuitem.setAttribute ("key", "key:" + command.name);
    menuitem.setAttribute ("accesskey", getAccessKey(command.label));
    var label = command.label.replace("&", "");
    menuitem.setAttribute ("label", label);
    if (command.format)
    {
        menuitem.setAttribute("format", command.format);
        menuitem.setAttribute("backupLabel", label);
    }
    menuitem.setAttribute ("oncommand", this.commandStr);

    if (typeof attribs == "object")
    {
        for (var p in attribs)
            menuitem.setAttribute (p, attribs[p]);
    }
    
    command.uiElements.push(menuitem);
    parentNode.insertBefore (menuitem, beforeNode);

    return menuitem;
}

/**
 * Appends a menuseparator to an existing menu or popup.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param attribs Object containing CSS attributes to set on the element.
 */
MenuManager.prototype.appendMenuSeparator =
function mmgr_addsep (parentNode, beforeNode, attribs)
{
    var document = parentNode.ownerDocument;
    var menuitem = document.createElement ("menuseparator");
    menuitem.setAttribute ("isSeparator", true);
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            menuitem.setAttribute (p, attribs[p]);
    }
    parentNode.insertBefore (menuitem, beforeNode);

    return menuitem;
}

/**
 * Appends a toolbaritem to an existing box element.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param command A reference to the CommandRecord this toolbaritem will
 *                represent.
 * @param attribs Object containing CSS attributes to set on the element.
 */
MenuManager.prototype.appendToolbarItem =
function mmgr_addtb (parentNode, beforeNode, commandName, attribs)
{
    if (commandName == "-")
        return this.appendToolbarSeparator(parentNode, beforeNode, attribs);

    var parentId = parentNode.getAttribute("id");
    
    if (!ASSERT(commandName in this.commandManager.commands,
                "unknown command " + commandName + " targeted for " +
                parentId))
    {
        return null;
    }
    
    var command = this.commandManager.commands[commandName];
    var document = parentNode.ownerDocument;    
    var tbitem = document.createElement ("toolbarbutton");

    var id = parentNode.getAttribute("id") + ":" + commandName;
    tbitem.setAttribute ("id", id);
    tbitem.setAttribute ("class", "toolbarbutton-1");
    if (command.tip)
        tbitem.setAttribute ("tooltiptext", command.tip);
    tbitem.setAttribute ("label", command.label.replace("&", ""));
    tbitem.setAttribute ("oncommand",
                         "dispatch('" + commandName + "');");
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            tbitem.setAttribute (p, attribs[p]);
    }
    
    command.uiElements.push(tbitem);
    parentNode.insertBefore (tbitem, beforeNode);

    return tbitem;
}

/**
 * Appends a toolbarseparator to an existing box.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param attribs Object containing CSS attributes to set on the element.
 */
MenuManager.prototype.appendToolbarSeparator =
function mmgr_addmenu (parentNode, beforeNode, attribs)
{
    var document = parentNode.ownerDocument;    
    var tbitem = document.createElement ("toolbarseparator");
    tbitem.setAttribute ("isSeparator", true);
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            tbitem.setAttribute (p, attribs[p]);
    }
    parentNode.appendChild (tbitem);

    return tbitem;
}

/**
 * Creates menu DOM nodes from a menu specification.
 * @param parentNode  DOM Node to insert into
 * @param beforeNode  DOM Node already contained by parentNode, to insert before
 * @param menuSpec    array of menu items
 */
MenuManager.prototype.createMenu =
function mmgr_newmenu (parentNode, beforeNode, menuName, domId, attribs)
{
    if (typeof domId == "undefined")
        domId = menuName;
    
    if (!ASSERT(menuName in this.menuSpecs, "unknown menu name " + menuName))
        return null;

    var menuSpec = this.menuSpecs[menuName];
    
    var subMenu = this.appendSubMenu (parentNode, beforeNode, menuName, domId,
                                      menuSpec.label, attribs);

    this.createMenuItems (subMenu, null, menuSpec.items);
    return subMenu;
}

MenuManager.prototype.createMenuItems =
function mmgr_newitems (parentNode, beforeNode, menuItems)
{
    function itemAttribs()
    {
        return (1 in menuItems[i]) ? menuItems[i][1] : null;
    };

    var parentId = parentNode.getAttribute("id");
    
    for (var i in menuItems)
    {
        var itemName = menuItems[i][0];
        if (itemName[0] == ">")
        {
            itemName = itemName.substr(1);
            if (!ASSERT(itemName in this.menuSpecs,
                        "unknown submenu " + itemName + " referenced in " +
                        parentId))
            {
                continue;
            }
            this.createMenu (parentNode, beforeNode, itemName,
                             parentId + ":" + itemName, itemAttribs());
        }
        else if (itemName in this.commandManager.commands)
        {
            this.appendMenuItem (parentNode, beforeNode, itemName,
                                 itemAttribs());
        }
        else if (itemName == "-")
        {
            this.appendMenuSeparator (parentNode, beforeNode, itemAttribs());
        }
        else
        {
            dd ("unknown command " + itemName + " referenced in " + parentId);
        }
    }
        
}
