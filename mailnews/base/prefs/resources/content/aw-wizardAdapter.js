/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var DEBUG_alecf = false;
function _dd(aString) 
{
  if (DEBUG_alecf)
    dump(aString);
} 
 
var gFieldList;
         
function GetFields()
{
    _dd("wizardAdapter: GetFields()\n");
    if (!gFieldList)
        gFieldList = document.getElementsByAttribute("wsm_persist", "true");

    var fields = new Object;
    for (var i=0; i<gFieldList.length; i++) {
        var field=gFieldList[i];

        if (field.parentNode.tagName == "template") continue;
        
        _dd("    for field <" + field.tagName + ">\n");
        var obj = new Object;
        obj.id = field.id;

        if (field.tagName == "radio" ||
            field.tagName == "checkbox")
            obj.value = field.checked;
        else if (field.tagName == "menulist")
            obj.value = field.selectedItem.data;
        else
            obj.value = field.value;
        
        _dd("    returning " + obj.id + " and " + obj.value + " value=" + field.value + "\n");
        fields[field.id] = obj;
    }

    return fields;
}

function SetFields(id, value)
{
    _dd("wizardAdapter: SetFields(" + id + ", " + value + ")\n");
    var field = document.getElementById(id);

    if (!field) {
        _dd("    Unknown field with id " + id + "\n");
        _dd("    Trying to find it in the ispbox \n");
        var ispBox = document.getElementById("ispBox");
        var fields = document.getElementsByAttribute("id", id);
        if (!fields || fields.length == 0) {
            _dd("still couldn't find it!\n");
            return;
        } else
            field = fields[0];

        DumpDOM(ispBox);
        return;
    }

    _dd("    SetFields(<" + field.tagName + ">);\n");
    if (field.tagName == "radio" ||
        field.tagName == "checkbox")
        field.checked = value;
    else if (field.tagName == "menulist") {
        var menuitems = field.getElementsByAttribute("data", value);
        if (menuitems && menuitems.length)
            field.selectedItem = menuitems[0];
    }
    else
        field.value = value;
}
