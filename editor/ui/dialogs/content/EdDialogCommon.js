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
 *   Pete Collins
 *   Brian King
 */

// Each editor window must include this file
// Variables  shared by all dialogs:
var editorShell;
var SelectionOnly = 1; 
var FormatedWithDoctype  = 2; 
var FormatedWithoutDoctype = 6; 
var maxPixels  = 10000;
// The element being edited - so AdvancedEdit can have access to it
var globalElement;

function InitEditorShell()
{
    // get the editor shell from the parent window

  editorShell           = window.opener.editorShell;
  if (editorShell) {
    editorShell         = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  }
  if (!editorShell) {
    dump("EditorShell not found!!!\n");
    window.close();
    return false;
  }

  // Save as a property of the window so it can be used by child dialogs

  window.editorShell         = editorShell;
  
  return true;
}

function StringExists(string)
{
  if (string != null && string != "undefined" && string.length > 0)
    return true;

  return false;
}

function ClearList(list)
{
  if (list) {
    list.selectedIndex = -1;
    for( i=list.length-1; i >= 0; i--)
      list.remove(i);
  }
}

function ReplaceStringInList(list, index, string)
{
  if (index < list.options.length)
  {
    // Save and remove selection else we have trouble below!
    //  (This must be a bug!)
    selIndex = list.selectedIndex;
    list.selectedIndex = -1;

    optionNode = new Option(string, string);
    // Remove existing option node
    //list.remove(index);
    list.options[index] = null;
    // Insert the new node
    list.options[index] = optionNode;
    // NOTE: If we insert, then remove, we crash!
    // Reset the selected item
    list.selectedIndex = selIndex;
  }
}

function AppendStringToListByID(list, stringID)
{
  AppendStringToList(list, editorShell.GetString(stringID));
}

function AppendStringToList(list, string)
{
  // THIS DOESN'T WORK! Result is a XULElement -- namespace problem
  // optionNode1 = document.createElement("option");
  // This works - Thanks to Vidur! Params = name, value
  optionNode = new Option(string, string);

  if (optionNode) {
    list.add(optionNode, null);    
  } else {
    dump("Failed to create OPTION node. String content="+string+"\n");
  }
}

/*
  It would be better to add method to Select object, but what is its name?
  SELECT and HTMLSELECT don't work!
*/

// "value" may be a number or string type

function ValidateNumberString(value, minValue, maxValue)
{
  // Get the number version (strip out non-numbers)

  value = value.replace(/\D+/g, "");
  number = value - 0;
  if ((value+"") != "") {
    if (number && number >= minValue && number <= maxValue ){

      // Return string version of the number

      return number + "";
    }
  }
  message = editorShell.GetString("ValidateNumber1")+number+editorShell.GetString("ValidateNumber2")+" "+minValue+" "+editorShell.GetString("ValidateNumber3")+" "+maxValue;
  ShowInputErrorMessage(message);

  // Return an empty string to indicate error

  return "";
}

function ShowInputErrorMessage(message)
{
  dump("ShowInputErrorMessage:"+message+"[end]\n");
  window.openDialog("chrome://editor/content/EdMessage.xul", "_blank", "chrome,close,titlebar,modal", "", message, "Input Error");
}

function GetString(name)
{
  return editorShell.GetString(name);
}

function TrimStringLeft(string)
{
  if(!string) return "";
  return string.replace(/^\s+/, "");
}

function TrimStringRight(string)
{
  if (!string) return "";
  return string.replace(/\s+$/, '');
}

// Remove whitespace from both ends of a string
function TrimString(string)
{
  if (!string) return "";
  return string.replace(/(^\s+)|(\s+$)/g, '')
}

String.prototype.trimString = function() {
  return this.replace(/(^\s+)|(\s+$)/g, '')
}

function IsWhitespace(string)
{
  return /^\s/.test(string);
}

function TruncateStringAtWordEnd(string, maxLength, addEllipses)
{
    // Return empty if string is null, undefined, or the empty string
    if (!string)
      return "";

    // We assume they probably don't want whitespace at the beginning
    string = string.replace(/^\s+/, '');
    if (string.length <= maxLength)
      return string;

    // We need to truncate the string to maxLength or fewer chars
    if (addEllipses) 
      maxLength -= 3; 
    string = string.replace(RegExp("(.{0," + maxLength + "})\\s.*"), "$1")

    if (addEllipses)
      string += "...";
    return string;
}

// Replace all whitespace characters with supplied character
// E.g.: Use charReplace = " ", to "unwrap" the string by removing line-end chars
//       Use charReplace = "_" when you don't want spaces (like in a URL)
function ReplaceWhitespace(string, charReplace) {
  return string.replace(/(^\s+)|(\s+$)/g,'').replace(/\s+/g,charReplace)
}

// Replace whitespace with "_" and allow only "word" characters (a-z,A-Z,0-9 and _)
function PrepareStringForURL(string)
{
  return ReplaceWhitespace(string,"_").replace(/\W+/g,'')
}

// this function takes an elementID and a flag
// if the element can be found by ID, then it is either enabled (by removing "disabled" attr)
// or disabled (setAttribute) as specified in the "doEnable" parameter
function SetElementEnabledByID( elementID, doEnable )
{
  element = document.getElementById(elementID);
  if ( element )
  {
//dump("*** SetElementEnabledByID: Element="+element+" ID="+elementID+"\n");
    if ( doEnable )
    {
      element.removeAttribute( "disabled" );
    }
    else
    {
      element.setAttribute( "disabled", "true" );
    }
  }
  else
  {
    dump("Element "+elementID+" not found in SetElementEnabledByID\n");
  }
}

// This function relies on css classes for enabling and disabling
// This function takes an ID for a label and a flag
// if an element can be found by its ID, then it is either enabled or disabled
// The class is set to either "enabled" or "disabled" depending on the flag passed in.
// This function relies on css having a special appearance for these two classes.

function SetClassEnabledByID( elementID, doEnable )
{
  element = document.getElementById(elementID);
  if ( element )
  {
    if ( doEnable )
    {
     element.setAttribute( "class", "enabled" );
    }
    else
    {
     element.setAttribute( "class", "disabled" );
    }
  }
  else
  {
    dump( "not changing element "+elementID+"\n" );
  }
}

// Get the text appropriate to parent container
//  that may be a cell or window
function GetAppropriatePercentString()
{
  var selection = window.editorShell.editorSelection;
  if (selection) {
    if (editorShell.GetElementOrParentByTagName("td",selection.focusNode))
      return GetString("PercentOfCell");
  }
  return GetString("PercentOfWindow");
}

// Returns the value for the "size" input element ("%" is stripped)
// Appends option elements with the correct strings to the select widget
function InitPixelOrPercentCombobox(element, attribute, selectID)
{
  size   = element.getAttribute(attribute);
  select = document.getElementById(selectID);

  if (select) {
    ClearList(select);
    AppendStringToList(select,GetString("Pixels"));
    AppendStringToList(select,GetAppropriatePercentString());
  }

  // Search for a "%" character
  percentIndex = size.search(/%/);
  if (percentIndex > 0) {
    // Strip out the %
    size = size.substr(0, percentIndex);
    if (select)
      select.selectedIndex = 1;
  } else {
    if (select)
      select.selectedIndex = 0;
  }
  return size;
}

// Next two methods assume caller has a "percentChar" variable 
//  to hold an empty string (pixels are used) or "%" (percent is used)

function InitPixelOrPercentPopupButton(element, attribute, buttonID)
{
  size = element.getAttribute(attribute);
  btn  = document.getElementById(buttonID);

  // Search for a "%" character

  percentIndex = size.search(/%/);
  if (percentIndex > 0) {
    percentChar = "%";
    // Strip out the %
    size = size.substr(0, percentIndex);

    if (btn)
      btn.setAttribute("value",GetAppropriatePercentString());
  } else {
    if (btn)
      btn.setAttribute("value",GetString("Pixels"));
  }
  return size;
}

// Input string is "" for pixel, or "%" for percent

function SetPixelOrPercentByID(elementID, percentString)
{
  percentChar = percentString;

  btn = document.getElementById( elementID );
  if ( btn )
  {
    if ( percentChar == "%" )
    {
      var containing = getContainer();
      if (containing != null)
      {
        if (containing.nodeName == "TD")
          btn.setAttribute("value", GetString("PercentOfCell"));
        else
          btn.setAttribute("value", GetString("PercentOfWindow"));
      }
    // need error handling
    }
    else
    {
      btn.setAttribute("value", GetString("Pixels"));
    }
  }
}

// USE onkeyup!
// forceInteger by petejc (pete@postpagan.com)

var sysBeep = Components.classes["component://netscape/sound"].createInstance();
sysBeep     = sysBeep.QueryInterface(Components.interfaces.nsISound);

function forceInteger(elementID)
{
  var editField = document.getElementById( elementID );
  if ( !editField )
    return;

  var stringIn = editField.value;
  var pat = /\D+/g;
  if (pat.test(stringIn)) {
    editField.value = stringIn.replace(pat,"");

    // we hope to remove the following line for blur() once xp widgets land
    // cmanske (9/15) testing this now that GFX ender widget is active
    //editField.blur();
    sysBeep.Beep();
  }
}


function onAdvancedEdit()
{
  // First validate data from widgets in the "simpler" property dialog
  if (ValidateData()) 
  {
    // Set true if OK is clicked in the Advanced Edit dialog
    window.AdvancedEditOK = false;
    // Open the AdvancedEdit dialog, passing in the element to be edited
    //  (the copy named "globalElement")
    window.openDialog("chrome://editor/content/EdAdvancedEdit.xul", "_blank", "chrome,close,titlebar,modal", "", globalElement);
    if (window.AdvancedEditOK) {
      // Copy edited attributes to the dialog widgets:
      //  to setup for validation
      InitDialog();
      // Try to just close the parent dialog as well,
      //  but this will do validation first
      if (onOK()) {
        // I'm not sure why, but calling onOK() from JS doesn't trigger closing
        //   automatically as it does when you click on the OK button!
        window.close();
      }
    }
    // Should we Cancel the parent dialog if user Cancels in AdvancedEdit?
  }
}

function onCancel()
{
  window.close();
}

function GetSelectionAsText()
{
  return editorShell.GetContentsAs("text/plain", SelectionOnly);
}


// ** getSelection () 
// ** This function checks for existence of table around the focus node
// ** Brian King - XML Workshop

function getContainer ()
{
  tagName = "img";
  selImage = editorShell.GetSelectedElement(tagName);
  if (selImage)  // existing image
  {
    dump("Is an image element\n");
    oneup = selImage.parentNode;
    dump("Parent = " + oneup.nodeName + "\n");
    //twoup = oneup.parentNode;
    //dump("Grand Parent is " + twoup.nodeName + "\n");
    return oneup;
  }
  else if (!selImage)  // new image insertion
  {
    dump("Not an image element\n");
    dump("Trying for caret selection\n");
    var selection = window.editorShell.editorSelection;
    if (selection)
    {
        dump("Got selection\n");
        //dump("Selection is " + selection.nodeName + "\n");
        var focusN = selection.focusNode;
        dump("FOCUS is now on " + focusN.nodeName + "\n");
        if (focusN.nodeName == "TD")
          return focusN
                else
        {
          oneup = focusN.parentNode;
          dump("PARENT is " + oneup.nodeName + "\n");
          return oneup;
         }  
    }
    else
      return null;
   }
   else
     return null;
}

// setColorWell assumes the following UI pattern:
/*
<menu>
	<titledbutton value="&????" class="popup" align="right"/>
The colorWell:
  <html:div style="width:30px; background-color:white"/> 
	<menupopup>
		<colorpicker palettename="standard" onclick="setColorWell(this.parentNode.parentNode);"/>
	</menupopup>
</menu>
*/
function setColorWell(menu)
{
  // Find the colorWell and colorPicker in the hierarchy.
  var colorWell = menu.firstChild.nextSibling;
  var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;

  // Extract color from colorPicker and assign to colorWell.
  var color = colorPicker.getAttribute('color');
  colorWell.style.backgroundColor = color;
}
