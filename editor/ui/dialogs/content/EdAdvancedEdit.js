/* 
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger
 */


//Cancel() is in EdDialogCommon.js
// Note: This dialog
var tagname;
var element;
var elAttrs = new Array();

// dialog initialization code
function Startup()
{
  dump("START DLG\n");
  // This is the return value for the parent,
  //  who only needs to know if OK was clicked
  window.opener.AdvancedEditOK = false;

  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  // Element to edit is passed in
  element = window.arguments[1];
  if (!element || element == "undefined") {
    dump("Advanced Edit: Element to edit not supplied\n");
    window.close();  
  }
  dump("*** Element passed into Advanced Edit: "+element+" ***\n");

  var tagLabel = document.getElementById("tagLabel");
  if(tagLabel.hasChildNodes()) {
    tagLabel.removeChild(tagLabel.firstChild);
  }

  var textLabel = document.createTextNode("<" + element.nodeName + ">");
  tagLabel.appendChild(textLabel);

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.AddAttributeNameInput = document.getElementById("AddAttributeNameInput");
  dialog.AddAttributeValueInput = document.getElementById("AddAttributeValueInput");
  dialog.AddAttribute = document.getElementById("AddAttribute");

  // build an attribute tree
  BuildAttributeTable();
  window.sizeToContent();
}

// build attribute list in tree form from element attributes
function BuildAttributeTable()
{
  dump("NODENAME: " + element.nodeName + "\n");
  var nodeMap = element.attributes;
  var nodeMapCount = nodeMap.length;
  var treekids = document.getElementById("attributelist");

  if(nodeMapCount > 0) {
      for(i = 0; i < nodeMapCount; i++)
      {
        if(!CheckAttributeNameSimilarity(nodeMap[i].nodeName)) {
            dump("repeated attribute!\n");
            continue;   // repeated attribute, ignore this one and go to next
        }
        elAttrs[i] = nodeMap[i].nodeName;
        var treeitem    = document.createElement("treeitem");
        var treerow     = document.createElement("treerow");
        var attrcell    = document.createElement("treecell");
        var attrcontent = document.createTextNode(nodeMap[i].nodeName.toUpperCase());
        attrcell.appendChild(attrcontent);
        treerow.appendChild(attrcell);
        var valcell     = document.createElement("treecell");
        valcell.setAttribute("class","value");
        var valField    = document.createElement("html:input");
        var attrValue   = element.getAttribute(nodeMap[i].nodeName);
        valField.setAttribute("type","text");
        valField.setAttribute("id",nodeMap[i].nodeName.toLowerCase());
        valField.setAttribute("value",attrValue);
        valField.setAttribute("flex","100%");
        valField.setAttribute("class","AttributesCell");
        valcell.appendChild(valField);
        treerow.appendChild(valcell);
        treeitem.appendChild(treerow);
        treekids.appendChild(treeitem);
      }
  }
}

// add an attribute to the tree widget
function onAddAttribute()
{
  //var name = TrimString(dialog.AddAttributeNameInput.value);
  name = dialog.AddAttributeNameInput.value;
  var value = TrimString(dialog.AddAttributeValueInput.value);

  // Must have a name to be able to add (Value may be empty)
  if(name == "")
    return;

  // WHAT'S GOING ON? NAME ALWAYS HAS A VALUE OF accented "a"???
  dump(name+"= New Attribute Name - SHOULD BE EMPTY\n");


  elAttrs[elAttrs.length] = name;
  
  // TODO: Add a new text + editbox to the treewidget editing list
  var treekids = document.getElementById("attributelist");
  var treeitem    = document.createElement("treeitem");
  var treerow     = document.createElement("treerow");
  var attrcell    = document.createElement("treecell");
  var attrcontent = document.createTextNode(name.toUpperCase());
  attrcell.appendChild(attrcontent);
  treerow.appendChild(attrcell);
  var valcell     = document.createElement("treecell");
  valcell.setAttribute("class","value");
  var valField    = document.createElement("html:input");
  valField.setAttribute("type","text");
  valField.setAttribute("id",name);
  valField.setAttribute("value",value);
  valField.setAttribute("flex","100%");
  valField.setAttribute("class","AttributesCell");
  valcell.appendChild(valField);
  treerow.appendChild(valcell);
  treeitem.appendChild(treerow);
  treekids.appendChild(treeitem);
  dialog.AddAttributeNameInput.value = "";
  dialog.AddAttributeValueInput.value = "";
  
  // Set focus to the value edit field just added:
  valField.focus();
  
  //Clear the edit boxes
  dialog.AddAttributeNameInput.value = "";
  dialog.AddAttributeValueInput.value = "";
}

// shut the dialog, apply changes and tidy up
function onOK()
{
  UpdateObject(); // call UpdateObject fn to update element in document
  window.opener.AdvancedEditOK = true;
  window.opener.globalElement = element;
  return true; // do close the window
}

// updates the element object with values set in the tree.
// TODO: make this work for many objects, so the "vicinity diagram"
//       can be used.
function UpdateObject()
{
  var treekids = document.getElementById("attributelist");
  for(i = 0; i < treekids.childNodes.length; i++)
  {
    var item = treekids.childNodes[i];
    var name = item.firstChild.firstChild.firstChild.nodeValue;
    var value = TrimString(item.firstChild.lastChild.firstChild.value);
    element.setAttribute(name,value);
  }
}

// checks to see if any other attributes by the same name as the arg supplied
// already exist.
function CheckAttributeNameSimilarity(attName)
{
    for(i = 0; i < elAttrs.length; i++)
    {
        if(attName == elAttrs[i]) 
            return false;
    }
    return true;
}

// does enabling based on any user input.
function doOverallEnabling()
{
    var name = TrimString(dialog.AddAttributeNameInput.value);
    if( name == "" || !CheckAttributeNameSimilarity(name)) {
        dialog.AddAttribute.setAttribute("disabled","true");
    } else {
        dialog.AddAttribute.removeAttribute("disabled");
    }
}

function doSort()
{
/*  UpdateObject();
  var treekids = document.getElementById("attributelist");
  var nameArray = []
  for(i = 0; i < treekids.childNodes.length; i++)
  {
    var item = treekids.childNodes[i];
    nameArray[i] = item.firstChild.firstChild.firstChild.nodeValue;
  }
  nameArray.sort(posval);
  dump("nameArray: " + nameArray + "\n");
  nameArray.sort(negval);
  dump("nameArray: " + nameArray + "\n");
*/
}

