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

/**************         GLOBALS         **************/
var tagname     = null;
var element     = null;

var HTMLAttrs   = [];   // html attributes
var CSSAttrs    = [];   // css attributes
var JSEAttrs    = [];   // js events 

/************** INITIALISATION && SETUP **************/
function Startup()
{
  dump("Welcome to EdAdvancedEdit '99 \n");
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
  dialog.AddHTMLAttributeNameInput = document.getElementById("AddHTMLAttributeNameInput");
  dialog.AddHTMLAttributeValueInput = document.getElementById("AddHTMLAttributeValueInput");
  dialog.AddHTMLAttribute = document.getElementById("AddHTMLAttribute");
  dialog.AddCSSAttributeNameInput = document.getElementById("AddCSSAttributeNameInput");
  dialog.AddCSSAttributeValueInput = document.getElementById("AddCSSAttributeValueInput");
  dialog.AddCSSAttribute = document.getElementById("AddCSSAttribute");
  dialog.AddJSEAttributeNameInput = document.getElementById("AddJSEAttributeNameInput");
  dialog.AddJSEAttributeValueInput = document.getElementById("AddJSEAttributeValueInput");
  dialog.AddJSEAttribute = document.getElementById("AddJSEAttribute");

  // build the attribute trees
  BuildHTMLAttributeTable();
  BuildCSSAttributeTable();
  BuildJSEAttributeTable();
  window.sizeToContent();
}

// for..in loop, typeof, /^on/ match


function onOK()
{
  dump("in onOK\n")
  UpdateObject(); // call UpdateObject fn to update element in document
  window.opener.AdvancedEditOK = true;
  window.opener.globalElement = element;
  return true; // do close the window
}

// function EdAvancedEdit.js::<UpdateObject> 
// Updates the copy of the page object with the data set in this dialog.
function UpdateObject()
{
  dump("in UpdateObject\n");
  var HTMLAList = document.getElementById("HTMLAList");
  var CSSAList = document.getElementById("CSSAList");
  var JSEAList = document.getElementById("JSEAList");
  
  // HTML ATTRIBUTES
  for(var i = 0; i < HTMLAList.childNodes.length; i++)
  {
    var item = HTMLAList.childNodes[i];
    var name = TrimString(item.firstChild.firstChild.firstChild.nodeValue);
    var value = TrimString(item.firstChild.lastChild.firstChild.value);
    dump("HTML Attrs: n: " + name + "; v: " + value + "\n");
    element.setAttribute(name,value);
  }
  // CSS ATTRIBUTES
  var styleString = "";
  for(var i = 0; i < CSSAList.childNodes.length; i++)
  {
    var item = CSSAList.childNodes[i];
    var name = TrimString(item.firstChild.firstChild.firstChild.nodeValue);
    var value = TrimString(item.firstChild.lastChild.firstChild.value);
    if(name.lastIndexOf(":") == (name.length - 1) && name.length > 0)
      name = name.substring(0,name.length-1);
    if(value.lastIndexOf(";") == (value.length - 1) && value.length > 0)
      value = name.substring(0,value.length-1);
    if(i == (CSSAList.childNodes.length - 1))
      styleString += name + ": " + value + ";";   // last property
    else
      styleString += name + ": " + value + "; ";
  }
  dump("stylestring: ||" + styleString + "||\n");
  var name = "width";
  if(styleString.length > 0) {
    element.setAttribute(name,styleString);
  }
  // JS EVENT HANDLERS
  for(var i = 0; i < JSEAList.childNodes.length; i++)
  {
    var item = JSEAList.childNodes[i];
    name = TrimString(item.firstChild.firstChild.firstChild.nodeValue);
    value = TrimString(item.firstChild.lastChild.firstChild.value);
    element.setAttribute(name,value);
  }
}

// checks to see if any other attributes by the same name as the arg supplied
// already exist.
function CheckAttributeNameSimilarity(attName, attArray)
{
    for(var i = 0; i < attArray.length; i++)
    {
        if(attName == attArray[i]) 
            return false;
    }
    return true;
}
