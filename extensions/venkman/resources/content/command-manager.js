/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */


function getAccessKey (str)
{
    var i = str.indexOf("&");
    if (i == -1)
        return "";
    return str[i + 1];
}

function CommandRecord (name, func, usage, help, label, flags)
{
    this.name = name;
    this.func = func;
    this._usage = usage;
    this.scanUsage();
    this.help = help;
    this.label = label ? label : name;
    this.flags = flags;
    this._enabled = true;
    this.key = null;
    this.keystr = MSG_VAL_NA;
    this.uiElements = new Object();
}

CommandRecord.prototype.__defineGetter__ ("enabled", cr_getenable);
function cr_getenable ()
{
    return this._enabled;
}

CommandRecord.prototype.__defineSetter__ ("enabled", cr_setenable);
function cr_setenable (state)
{
    for (var i in this.uiElements)
    {
        if (state)
            this.uiElements[i].removeAttribute ("disabled");
        else
            this.uiElements[i].setAttribute ("disabled", "true");
    }
    return (this._enabled = state);
}

CommandRecord.prototype.__defineSetter__ ("usage", cr_setusage);
function cr_setusage (usage)
{
    this._usage = usage;
    this.scanUsage();
}

CommandRecord.prototype.__defineGetter__ ("usage", cr_getusage);
function cr_getusage()
{
    return this._usage;
}

/**
 * Internal use only.
 *
 * Scans the argument spec, in the format "<a1> <a2> [<o1> <o2>]", into an
 * array of strings.
 */
CommandRecord.prototype.scanUsage =
function cr_scanusage()
{
    var spec = this._usage;
    var currentName = "";
    var inName = false;
    var len = spec.length;
    var capNext = false;
    
    this._usage = spec;
    this.argNames = new Array();    

    for (var i = 0; i < len; ++i)
    {
        switch (spec[i])
        {
            case '[':
                this.argNames.push (":");
                break;
                
            case '<':
                inName = true;
                break;
                
            case '-':
                capNext = true;
                break;
                
            case '>':
                inName = false;
                this.argNames.push (currentName);
                currentName = "";
                capNext = false;
                break;
                
            default:
                if (inName)
                    currentName += capNext ? spec[i].toUpperCase() : spec[i];
                capNext = false;
                break;
        }
    }
}

/**
 * Returns the command formatted for presentation as part of online
 * documentation.
 */
CommandRecord.prototype.getDocumentation =
function cr_getdocs(flagFormatter)

{
    var str;
    str  = getMsg(MSN_DOC_COMMANDLABEL,
                  [this.label.replace("&", ""), this.name]) + "\n";
    str += getMsg(MSN_DOC_KEY, this.keystr) + "\n";
    str += getMsg(MSN_DOC_SYNTAX, [this.name, this.usage]) + "\n";
    str += MSG_DOC_NOTES + "\n";
    str += (flagFormatter ? flagFormatter(this.flags) : this.flags) + "\n\n";
    str += MSG_DOC_DESCRIPTION + "\n";
    str += wrapText(this.help, 75) + "\n";
    return str;
}

CommandRecord.prototype.argNames = new Array();

function CommandManager ()
{
    this.commands = new Object();
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
CommandManager.showPopup =
function cmgr_showpop (id)
{
    /* returns true if the command context has the properties required to
     * execute the command associated with |menuitem|.
     */
    function satisfied()
    {
        if (menuitem.hasAttribute("isSeparator"))
            return true;

        if (!("commandManager" in cx))
        {
            dd ("no commandManager in cx");
            return false;
        }
        
        var name = menuitem.getAttribute("commandname");
        if (!ASSERT (name in cx.commandManager.commands,
                     "menu contains unknown command '" + name + "'"))
            return false;

        var command = cx.commandManager.commands[name];

        var rv = cx.commandManager.isCommandSatisfied(cx, command);
        delete cx.parseError;
        return rv;
    }
    
    /* Convenience function for "enabledif", etc, attributes. */
    function has (prop)
    {
        return (prop in cx);
    }
    
    /* evals the attribute named |attr| on the node |node|. */
    function evalIfAttribute (node, attr)
    {
        var ex;
        var expr = node.getAttribute(attr);
        if (!expr)
            return true;
        
        expr = expr.replace (/\Wor\W/gi, " || ");
        expr = expr.replace (/\Wand\W/gi, " && ");
        try
        {
            return eval("(" + expr + ")");
        }
        catch (ex)
        {
            dd ("caught exception evaling '" + node.getAttribute("id") + "'.'" +
                attr + "'\n" + ex);
        }
        return true;
    }
    
    var cx;
    
    /* If the host provided a |contextFunction|, use it now.  Remember the
     * return result as this.cx for use if something from this menu is actually
     * dispatched.  this.cx is deleted in |hidePopup|. */
    if (typeof this.contextFunction == "function")
        cx = this.cx = this.contextFunction (id);
    else
        cx = new Object();

    /* make this a member of cx so the |this| value is useful to attriutes. */

    var popup = document.getElementById (id);
    var menuitem = popup.firstChild;

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

    return true;
}

/**
 * Internal use only.
 *
 * |hidePopup| is called from the "onpopuphiding" event of menus
 * managed by the CommandManager.  Here we just clean up the context, which
 * would have (and may have) become the event object used by any command
 * dispatched while the menu was open.
 */
CommandManager.hidePopup =
function cmgr_hidepop (id)
{
    delete this.cx;
}

/**
 * Register a new command with the manager.
 */
CommandManager.prototype.addCommand =
function cmgr_add (command)
{
    this.commands[command.name] = command;
}

/**
 * Set the accelerator key used to trigger a command.  Multiple keys can be
 * assigned to a command, but only the last one will appear in the documentation
 * entry for the command.
 * @param parent   ID of the keyset to add the <key> to.
 * @param command  reference to the CommandRecord which should be dispatced.
 * @param keystr   String representation of the key, in the format
 *                 <modifiers> (<key>|<keycode>), where the parameter names
 *                 represent attributes on the <key> object.
 */
CommandManager.prototype.setKey =
function cmgr_add (parent, command, keystr)
{
    var parentElem = document.getElementById(parent);
    if (!ASSERT(parentElem, "setKey: couldn't get parent '" + parent +
                "' for " + command.name))
        return;

    var ary = keystr.match (/(.*\s)?([\S]+)$/);
    var key = document.createElement ("key");
    key.setAttribute ("id", "key:" + command.name);
    key.setAttribute ("oncommand",
                      "dispatch('" + command.name + "');");
    key.setAttribute ("modifiers", ary[1]);
    if (ary[2].indexOf("VK_") == 0)
        key.setAttribute ("keycode", ary[2]);
    else
        key.setAttribute ("key", ary[2]);
    
    parentElem.appendChild(key);
    command.key = key;
    command.keystr = keystr;
}

/**
 * Internal use only.
 *
 * Registers event handlers on a given menu.
 */
CommandManager.prototype.hookPopup =
function cmgr_hookpop (id)
{
    var element = document.getElementById (id);
    element.setAttribute ("onpopupshowing",
                          "return CommandManager.showPopup('" + id + "');");
    element.setAttribute ("onpopuphiding",
                          "return CommandManager.hidePopup();");
}

/**
 * Appends a sub-menu to an existing menu.
 * @param parent  ID of the parent menu to add this submenu to.
 * @param id      ID of the sub-menu to add.
 * @param label   Text to use for this sub-menu.  The & character can be
 *                used to indicate the accesskey.
 * @param attribs Object containing CSS attributes to set on the element.
 */
CommandManager.prototype.appendSubMenu =
function cmgr_addsmenu (parent, id, label, attribs)
{
    var menu = document.getElementById (id);
    if (!menu)
    {
        var parentElem = document.getElementById(parent + "-popup");
        if (!parentElem)
            parentElem = document.getElementById(parent);
        
        if (!ASSERT(parentElem, "addSubMenu: couldn't get parent '" + parent +
                    "' for " + id))
            return;
    
        menu = document.createElement ("menu");
        menu.setAttribute ("id", id);
        parentElem.appendChild(menu);
    }
    
    menu.setAttribute ("accesskey", getAccessKey(label));
    menu.setAttribute ("label", label.replace("&", ""));
    menu.setAttribute ("isSeparator", true);
    var menupopup = document.createElement ("menupopup");
    menupopup.setAttribute ("id", id + "-popup");
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            menupopup.setAttribute (p, attribs[p]);
    }

    menu.appendChild(menupopup);    
    this.hookPopup (id + "-popup");
}

/**
 * Appends a popup to an existing popupset.
 * @param parent  ID of the popupset to add this popup to.
 * @param id      ID of the popup to add.
 * @param label   Text to use for this popup.  Popup menus don't normally have
 *                labels, but we set a "label" attribute anyway, in case
 *                the host wants it for some reason.  Any "&" characters will
 *                be stripped.
 * @param attribs Object containing CSS attributes to set on the element.
 */
CommandManager.prototype.appendPopupMenu =
function cmgr_addsmenu (parent, id, label, attribs)
{
    var parentElem = document.getElementById (parent);
    if (!ASSERT(parentElem, "addPopupMenu: couldn't get parent '" + parent +
                "' for " + id))
        return;

    var popup = document.createElement ("popup");
    popup.setAttribute ("label", label.replace("&", ""));
    popup.setAttribute ("id", id);
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            popup.setAttribute (p, attribs[p]);
    }

    parentElem.appendChild(popup);    
    this.hookPopup (id);
}

/**
 * Appends a menuitem to an existing menu or popup.
 * @param parent  ID of the popup to add this menuitem to.
 * @param command A reference to the CommandRecord this menu item will represent.
 * @param attribs Object containing CSS attributes to set on the element.
 */
CommandManager.prototype.appendMenuItem =
function cmgr_addmenu (parent, command, attribs)
{
    if (command == "-")
    {
        this.appendMenuSeparator(parent, attribs);
        return;
    }
    
    var parentElem = document.getElementById(parent + "-popup");
    if (!parentElem)
        parentElem = document.getElementById(parent);
    
    if (!ASSERT(parentElem, "appendMenuItem: couldn't get parent '" + parent +
                "' for " + command.name))
        return;
    
    var menuitem = document.createElement ("menuitem");
    var id = parent + ":" + command.name;
    menuitem.setAttribute ("id", id);
    menuitem.setAttribute ("commandname", command.name);
    menuitem.setAttribute ("key", "key:" + command.name);
    menuitem.setAttribute ("accesskey", getAccessKey(command.label));
    menuitem.setAttribute ("label", command.label.replace("&", ""));
    menuitem.setAttribute ("oncommand",
                           "dispatch('" + command.name + "');");
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            menuitem.setAttribute (p, attribs[p]);
    }
    
    command.uiElements[id] = menuitem;
    parentElem.appendChild (menuitem);
}

/**
 * Appends a menuseparator to an existing menu or popup.
 * @param parent  ID of the popup to add this menuitem to.
 */
CommandManager.prototype.appendMenuSeparator =
function cmgr_addsep (parent)
{
    var parentElem = document.getElementById(parent + "-popup");
    if (!parentElem)
        parentElem = document.getElementById(parent);
    
    if (!ASSERT(parentElem, "appendMenuSeparator: couldn't get parent '" + 
                parent + "'"))
        return;
    
    var menuitem = document.createElement ("menuseparator");
    menuitem.setAttribute ("isSeparator", true);
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            menuitem.setAttribute (p, attribs[p]);
    }
    parentElem.appendChild (menuitem);
}

/**
 * Appends a toolbaritem to an existing box element.
 * @param parent  ID of the box to add this toolbaritem to.
 * @param command A reference to the CommandRecord this toolbaritem will
 *                represent.
 * @param attribs Object containing CSS attributes to set on the element.
 */
CommandManager.prototype.appendToolbarItem =
function cmgr_addtb (parent, command, attribs)
{
    if (command == "-")
    {
        this.appendToolbarSeparator(parent, attribs);
        return;
    }

    var parentElem = document.getElementById(parent);
    
    if (!ASSERT(parentElem, "appendToolbarItem: couldn't get parent '" + parent +
                "' for " + command.name))
        return;
    
    var tbitem = document.createElement ("toolbarbutton");
    // separate toolbar id's with a "-" character, because : intereferes with css
    var id = parent + "-" + command.name;
    tbitem.setAttribute ("id", id);
    tbitem.setAttribute ("class", "toolbarbutton-1");
    tbitem.setAttribute ("label", command.label.replace("&", ""));
    tbitem.setAttribute ("oncommand",
                         "dispatch('" + command.name + "');");
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            tbitem.setAttribute (p, attribs[p]);
    }
    
    command.uiElements[id] = tbitem;
    parentElem.appendChild (tbitem);
}

/**
 * Appends a toolbarseparator to an existing box.
 * @param parent  ID of the box to add this toolbarseparator to.
 */
CommandManager.prototype.appendToolbarSeparator =
function cmgr_addmenu (parent)
{
    var parentElem = document.getElementById(parent);    
    if (!ASSERT(parentElem, "appendToolbarSeparator: couldn't get parent '" + 
                parent + "'"))
        return;
    
    var tbitem = document.createElement ("toolbarseparator");
    tbitem.setAttribute ("isSeparator", true);
    if (typeof attribs == "object")
    {
        for (var p in attribs)
            tbitem.setAttribute (p, attribs[p]);
    }
    parentElem.appendChild (tbitem);
}

/**
 * Return an array of all CommandRecords which start with the string
 * |partialName|, sorted by |label| property.
 *
 * @param   partialName Prefix to search for.
 * @param   flags       logical ANDed with command flags.
 * @returns array       Array of matching commands, sorted by |label| property.
 */
CommandManager.prototype.list =
function cmgr_list (partialName, flags)
{
    /* returns array of command objects which look like |partialName|, or
     * all commands if |partialName| is not specified */
    function compare (a, b)
    {
        a = a.label.toLowerCase().replace("&", "");
        b = b.label.toLowerCase().replace("&", "");

        if (a == b)
            return 0;
 
        if (a > b)
            return 1;

        return -1;
    }

    var ary = new Array();
    var commandNames = keys(this.commands);

    /* a command named "eval" wouldn't show up in the result of keys() because
     * eval is not-enumerable, even if overwritten. */
    if ("eval" in this.commands && typeof this.commands.eval == "object")
        commandNames.push ("eval");

    for (var i in commandNames)
    {
        var name = commandNames[i];
        if (!flags || (this.commands[name].flags & flags))
        {
            if (!partialName ||
                this.commands[name].name.indexOf(partialName) == 0)
            {
                if (partialName && 
                    partialName.length == this.commands[name].name.length)
                {
                    /* exact match */
                    return [this.commands[name]];
                }
                else
                {
                    ary.push (this.commands[name]);
                }
            }
        }
    }

    ary.sort(compare);
    return ary;
}

/**
 * Return a sorted array of the command names which start with the string
 * |partialName|.
 *
 * @param   partialName Prefix to search for.
 * @param   flags       logical ANDed with command flags.
 * @returns array       Sorted Array of matching command names.
 */
CommandManager.prototype.listNames =
function cmgr_listnames (partialName, flags)
{
    var cmds = this.list(partialName, flags);
    var cmdNames = new Array();
    
    for (var c in cmds)
        cmdNames.push (cmds[c].name);

    return cmdNames.sort();
}

/**
 * Internal use only.
 *
 * Called to parse the arguments stored in |e.inputData|, as properties of |e|,
 * for the CommandRecord stored on |e.command|.
 *
 * @params e  Event object to be processed.
 */
CommandManager.prototype.parseArguments =
function cmgr_parseargs (e)
{
    var rv = this.parseArgumentsRaw(e);
    //dd("parseArguments '" + e.command.usage + "' " + 
    //   (rv ? "passed" : "failed") + "\n" + dumpObjectTree(e));
    delete e.currentArgIndex;
    return rv;
}

/**
 * Internal use only.
 *
 * Don't call parseArgumentsRaw directly, use parseArguments instead.
 *
 * Parses the arguments stored in the |inputData| property of the event object,
 * according to the format specified by the |command| property.
 *
 * On success this method returns true, and propery names corresponding to the
 * argument names used in the format spec will be created on the event object.
 * All optional parameters will be initialized to |null| if not already present
 * on the event.
 *
 * On failure this method returns false and a description of the problem
 * will be stored in the |parseError| property of the event.
 *
 * For example...
 * Given the argument spec "<int> <word> [ <word2> <word3> ]", and given the
 * input string "411 foo", stored as |e.command.usage| and |e.inputData|
 * respectively, this method would add the following propertys to the event
 * object...
 *   -name---value--notes-   
 *   e.int    411   Parsed as an integer
 *   e.word   foo   Parsed as a string
 *   e.word2  null  Optional parameters not specified will be set to null.
 *   e.word3  null  If word2 had been provided, word3 would be required too.
 *
 * Each parameter is parsed by calling the function with the same name, located
 * in this.argTypes.  The first parameter is parsed by calling the function
 * this.argTypes["int"], for example.  This function is expected to act on
 * e.unparsedData, taking it's chunk, and leaving the rest of the string.
 * The default parse functions are...
 *   <word>    parses contiguous non-space characters.
 *   <int>     parses as an int.
 *   <rest>    parses to the end of input data.
 *   <state>   parses yes, on, true, 1, 0, false, off, no as a boolean.
 *   <toggle>  parses like a <state>, except allows "toggle" as well.
 *   <...>     parses according to the parameter type before it, until the end
 *             of the input data.  Results are stored in an array named
 *             paramnameList, where paramname is the name of the parameter before
 *             <...>.  The value of the parameter before this will be
 *             paramnameList[0].
 *
 * If there is no parse function for an argument type, "word" will be used by
 * default.  You can alias argument types with code like...
 * commandManager.argTypes["my-integer-name"] = commandManager.argTypes["int"];
 */
CommandManager.prototype.parseArgumentsRaw =
function parse_parseargsraw (e)
{
    var argc = e.command.argNames.length;
    
    function initOptionals()
    {
        for (var i = 0; i < argc; ++i)
        {
            if (e.command.argNames[i] != ":" && 
                e.command.argNames[i] != "..."  && 
                !(e.command.argNames[i] in e))
            {
                e[e.command.argNames[i]] = null;
            }
        }
    }
        
    if (e.inputData)
    {
        /* if data has been provided, parse it */
        e.unparsedData = e.inputData;
        var parseResult;
        var currentArg;
        e.currentArgIndex = 0;

        if (argc)
        {
            currentArg = e.command.argNames[e.currentArgIndex];
        
            while (e.unparsedData)
            {
                if (currentArg != ":")
                {
                    if (!this.parseArgument (e, currentArg))
                        return false;
                }
                if (++e.currentArgIndex < argc)
                    currentArg = e.command.argNames[e.currentArgIndex];
                else
                    break;
            }

            if (e.currentArgIndex < argc && currentArg != ":")
            {
                /* parse loop completed because it ran out of data.  We haven't
                 * parsed all of the declared arguments, and we're not stopped
                 * at an optional marker, so we must be missing something
                 * required... */
                e.parseError = getMsg(MSN_ERR_REQUIRED_PARAM, 
                                      e.command.argNames[e.currentArgIndex]);
                return false;
            }
        }
        
        if (e.unparsedData)
        {
            /* parse loop completed with unparsed data, which means we've
             * successfully parsed all arguments declared.  Whine about the
             * extra data... */
            display (getMsg(MSN_EXTRA_PARAMS, e.unparsedData), MT_WARN);
        }
        else 
        {
            /* we've got no unparsed data, and we're not missing a required
             * argument, go back and fill in |null| for any optional arguments
             * not present on the comand line. */
            initOptionals();
        }
    }
    else
    {
        /* if no data was provided, check to see if the event is good enough
         * on its own. */
        var rv = this.isCommandSatisfied (e);
        if (rv)
            initOptionals();
        return rv;
    }

    return true;
}

/**
 * Returns true if |e| has the properties required to call the command |command|.
 * If |command| is not provided, |e.command| is used instead.
 * @param e        Event object to test against the command.
 * @param command  Command to text.
 */
CommandManager.prototype.isCommandSatisfied =
function cmgr_isok (e, command)
{
    if (typeof command == "undefined")
        command = e.command;
    
    if (!command.enabled)
        return false;

    for (var i = 0; i < command.argNames.length; ++i)
    {
        if (command.argNames[i] == ":")
            return true;
        
        if (!(command.argNames[i] in e))
        {
            e.parseError = getMsg(MSN_ERR_REQUIRED_PARAM, command.argNames[i]);
            //dd ("command '" + command.name + "' unsatisfied: " + e.parseError);
            return false;
        }
    }

    //dd ("command '" + command.name + "' satisfied.");
    return true;
}

/**
 * Internally use only.
 * See parseArguments above and the |argTypes| object below.
 *
 * Parses the next argument by calling an appropriate parser function, or the
 * generic "word" parser if none other is found.
 *
 * @param e     event object.
 * @param name  property name to use for the parse result.
 */
CommandManager.prototype.parseArgument =
function cmgr_parsearg (e, name)
{
    var parseResult;
    
    if (name in this.argTypes)
        parseResult = this.argTypes[name](e, name, this);
    else
        parseResult = this.argTypes["word"](e, name, this);

    if (!parseResult)
        e.parseError = getMsg(MSN_ERR_INVALID_PARAM,
                              [name, e.unparsedData]);

    return parseResult;
}

CommandManager.prototype.argTypes = new Object();

/**
 * Convenience function used to map a list of new types to an existing parse
 * function.
 */
CommandManager.prototype.argTypes.__aliasTypes__ =
function at_alias (list, type)
{
    for (var i in list)
    {
        this[list[i]] = this[type];
    }
}

/**
 * Internal use only.
 *
 * Parses an integer, stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["int"] =
function parse_number (e, name)
{
    var ary = e.unparsedData.match (/(\d+)(?:\s+(.*))?$/);
    if (!ary)
        return false;
    e[name] = Number(ary[1]);
    e.unparsedData = (2 in ary) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses a word, which is defined as a list of nonspace characters.
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["word"] =
function parse_number (e, name)
{
    var ary = e.unparsedData.match (/(\S+)(?:\s+(.*))?$/);
    if (!ary)
        return false;
    e[name] = ary[1];
    e.unparsedData = (2 in ary) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses a "state" which can be "true", "on", "yes", or 1 to indicate |true|,
 * or "false", "off", "no", or 0 to indicate |false|.
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["state"] =
function parse_number (e, name)
{
    var ary =
        e.unparsedData.match (/(true|on|yes|1|false|off|no|0)(?:\s+(.*))?$/i);
    if (!ary)
        return false;
    if (ary[1].toLowerCase().search(/true|on|yes|1/i) != -1)
        e[name] = true;
    else
        e[name] = false;
    e.unparsedData = (2 in ary) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses a "toggle" which can be "true", "on", "yes", or 1 to indicate |true|,
 * or "false", "off", "no", or 0 to indicate |false|.  In addition, the string
 * "toggle" is accepted, in which case |e[name]| will be the string "toggle".
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["toggle"] =
function parse_number (e, name)
{
    var ary = e.unparsedData.match
        (/(toggle|true|on|yes|1|false|off|no|0)(?:\s+(.*))?$/i);

    if (!ary)
        return false;
    if (ary[1].search(/toggle/i) != -1)
        e[name] = "toggle";
    else if (ary[1].search(/true|on|yes|1/i) != -1)
        e[name] = true;
    else
        e[name] = false;
    e.unparsedData = (2 in ary) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Returns all unparsed data to the end of the line.
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["rest"] =
function parse_number (e, name)
{
    e[name] = e.unparsedData;
    e.unparsedData = "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses the rest of the unparsed data the same way the previous argument was
 * parsed.  Can't be used as the first parameter.  if |name| is "..." then the
 * name of the previous argument, plus the suffix "List" will be used instead.
 *
 * Stores result in |e[name]| or |e[lastName + "List"]|.
 */
CommandManager.prototype.argTypes["..."] =
function parse_repeat (e, name, cm)
{
    ASSERT (e.currentArgIndex > 0, "<...> can't be the first argument.");
    
    var lastArg = e.command.argNames[e.currentArgIndex - 1];
    if (lastArg == ":")
        lastArg = e.command.argNames[e.currentArgIndex - 2];

    var listName = lastArg + "List";
    e[listName] = [e[lastArg]];
    
    while (e.unparsedData)
    {
        if (!cm.parseArgument(e, lastArg))
            return false;
        e[listName].push(e[lastArg]);
    }    

    e[lastArg] = e[listName][0];
    return true;
}
