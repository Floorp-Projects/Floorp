/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is
 * ____________________________________________.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Ross, <twpol@aol.com>, original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function Init()
{
    parent.initPanel("chrome://chatzilla/content/prefpanel/startup.xul");
    loadList("Scripts");
    loadList("URLs");
}

function loadList(list)
{
    var gList = document.getElementById("czInitial" + list);
    
    var prefList = gList.getAttribute("prefvalue");
    
    if (prefList)
    {
        var items = prefList.split(/\s*;\s*/);
        for (i in items)
            listAddItem(list, items[i]);
    }
    
    listUpdateButtons(list);
}

function getIRCDisplayText(url)
{
    var params;
    
    // NOTE: This code (by design) ignores all the irc:// modifiers except 
    // 'isnick'. Showing the others can be added later, if needed.
    
    // FIXME: Localization support here (getMsg).
    
    if ((url == "irc:" || url == "irc:/" || url == "irc://"))
        return "Client view";
    
    if ((url == "irc:///"))
        return "Default network";
    
    if ((params = url.match(/^irc:\/\/\/([^,]*)(.*),isnick(,.*)?$/)))
        return params[2] + " nickname on default network";
    
    if ((params = url.match(/^irc:\/\/\/([^,]+)/)))
        return params[1] + " channel on default network";
    
    if ((params = url.match(/^irc:\/\/(.*)\/(,.*)?$/)))
        return params[1] + " network";
    
    if ((params = url.match(/^irc:\/\/(.*)\/([^,]*)(.*),isnick(,.*)?$/)))
        return params[2] + " nickname on " + params[1];
    
    if ((params = url.match(/^irc:\/\/(.*)\/([^,]+)/)))
        return params[2] + " channel on " + params[1];
    
    return url;
}

function addScript()
{
    try
    {
        var fpClass = Components.classes["@mozilla.org/filepicker;1"];
        var fp = fpClass.createInstance(nsIFilePicker);
        fp.init(window, getMsg("file_browse_Script"), nsIFilePicker.modeOpen);
        
        fp.appendFilter(getMsg("file_browse_Script_spec"), "*.js");
        fp.appendFilters(nsIFilePicker.filterAll);
        
        if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && 
                fp.fileURL.spec.length > 0)
            listAddItem("Scripts", fp.fileURL.spec);
    }
    catch(ex)
    {
        dump ("caught exception in `addScript`\n" + ex);
    }
}

function addURL()
{
    var gData = new Object();
    
    window.openDialog("chrome://chatzilla/content/prefpanel/startup-newURL.xul", 
            "czAddURL", "chrome,modal,resizable=no", gData);
    
    if (gData.ok)
        listAddItem("URLs", gData.url);
}

function editURL()
{
    var gData = new Object();
    var gList = document.getElementById("czInitialURLs");
    
    if (gList.selectedItems.length == 0)
        return;
    
    var selected = gList.selectedItems[0];
    
    gData.url = selected.getAttribute("url");
    
    window.openDialog("chrome://chatzilla/content/prefpanel/startup-newURL.xul", 
            "czEditURL", "chrome,modal,resizable=no", gData);
    
    if (gData.ok)
    {
        selected.setAttribute("label", getIRCDisplayText(gData.url));
        selected.setAttribute("url", gData.url);
    }
}

function listUpdateButtons(list)
{
    var gList = document.getElementById("czInitial" + list);
    var gButtonUp = document.getElementById("czUp" + list);
    var gButtonDn = document.getElementById("czDn" + list);
    var gButtonAdd = document.getElementById("czAdd" + list);
    var gButtonEdit = document.getElementById("czEdit" + list);
    var gButtonDel = document.getElementById("czDel" + list);
    
    if (gList.selectedItems && gList.selectedItems.length)
    {
        if (gButtonEdit) gButtonEdit.removeAttribute("disabled");
        gButtonDel.removeAttribute("disabled");
    }
    else
    {
        if (gButtonEdit) gButtonEdit.setAttribute("disabled", "true");
        gButtonDel.setAttribute("disabled", "true");
    }
    if (gList.selectedItems.length == 0 || gList.selectedIndex == 0)
    {
        gButtonUp.setAttribute("disabled", "true");
    }
    	else
    {
        gButtonUp.removeAttribute("disabled");
    }
    if (gList.selectedItems.length == 0 || 
            gList.selectedIndex == gList.childNodes.length - 1)
    {
        gButtonDn.setAttribute("disabled", "true");
    }
    else
    {
        gButtonDn.removeAttribute("disabled");
    }
}

function listUpdate(list)
{
    var gList = document.getElementById("czInitial" + list);
    
    listUpdateButtons(list);
    
    var prefList = "";
    
    for (var item = gList.firstChild; item; item = item.nextSibling)
    {
        var url = item.getAttribute("url");
        if (prefList)
            prefList = prefList + "; " + url;
        else
            prefList = url;
    }
    gList.setAttribute("prefvalue", prefList);
}

function listAdd(list)
{
    if (list == "Scripts")
        addScript();
    else if (list == "URLs")
        addURL();
    
    listUpdate(list);
}

function listEdit(list)
{
    if (list == "URLs")
        editURL();
    
    listUpdate(list);
}

function listDelete(list)
{
    var gList = document.getElementById("czInitial" + list);
    
    var selected = gList.selectedItems[0];
    gList.removeChild(selected);
    
    listUpdate(list);
}

function listAddItem(list, url)
{
    var gList = document.getElementById("czInitial" + list);
    var newItem = document.createElement("listitem");
    
    var label = url;
    if (list == "URLs")
        label = getIRCDisplayText(url);
    
    if (list == "Scripts")
        newItem.setAttribute("crop", "center");
    
    newItem.setAttribute("label", label);
    newItem.setAttribute("url", url);
    
    gList.appendChild(newItem);
}

function listMoveUp(list)
{
    var gList = document.getElementById("czInitial" + list);
    
    var selected = gList.selectedItems[0];
    var before = selected.previousSibling;
    if (before)
    {
        before.parentNode.insertBefore(selected, before);
        gList.selectItem(selected);
        gList.ensureElementIsVisible(selected);
    }
    listUpdate(list);
}

function listMoveDown(list)
{
    var gList = document.getElementById("czInitial" + list);
    
    var selected = gList.selectedItems[0];
    if (selected.nextSibling)
    {
        if (selected.nextSibling.nextSibling)
            gList.insertBefore(selected, selected.nextSibling.nextSibling);
        else
            gList.appendChild(selected);
        
        gList.selectItem(selected);
    }
    listUpdate(list);
}
