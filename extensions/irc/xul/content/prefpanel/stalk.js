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

function Init()
{
    parent.initPanel("chrome://chatzilla/content/prefpanel/stalk.xul");
    loadList("StalkWords");
}

function loadList(list)
{
    var gList = document.getElementById("cz" + list);
    
    var prefList = gList.getAttribute("prefvalue");
    
    if (prefList)
    {
        var items = prefList.split(/\s*;\s*/);
        for (i in items)
            listAddItem(list, items[i]);
    }
    
    listUpdateButtons(list);
}

function addStalkWord()
{
    var word = prompt(getMsg("stalk_add_msg"));
    
    if (word)
        listAddItem("StalkWords", word);
}

function listUpdateButtons(list)
{
    var gList = document.getElementById("cz" + list);
    var gButtonAdd = document.getElementById("czAdd" + list);
    var gButtonDel = document.getElementById("czDel" + list);
    
    if (gList.selectedItems && gList.selectedItems.length)
        gButtonDel.removeAttribute("disabled");
    else
        gButtonDel.setAttribute("disabled", "true");
}

function listUpdate(list) {
    var gList = document.getElementById("cz" + list);
    
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

function listAdd(list) {
    if (list == "StalkWords")
        addStalkWord();

    listUpdate(list);
}

function listDelete(list)
{
    var gList = document.getElementById("cz" + list);
    
    var selected = gList.selectedItems[0];
    gList.removeChild(selected);
    
    listUpdate(list);
}

function listAddItem(list, url)
{
    var gList = document.getElementById("cz" + list);
    var newItem = document.createElement("listitem");
    
    var label = url;
    
    newItem.setAttribute("label", label);
    newItem.setAttribute("url", url);
    
    gList.appendChild(newItem);
}

function listMoveUp(list)
{
    var gList = document.getElementById("cz" + list);
    
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
    var gList = document.getElementById("cz" + list);
    
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
