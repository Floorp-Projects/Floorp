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
// For dialogs that expand in size. Default is smaller size see "onMoreFewer()" below
var SeeMore = false;

// The element being edited - so AdvancedEdit can have access to it
var globalElement;

function InitEditorShell()
{
    // get the editor shell from the parent window

  editorShell = window.opener.editorShell;
  if (editorShell) {
    editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  }
  if (!editorShell) {
    dump("EditorShell not found!!!\n");
    window.close();
    return false;
  }

  // Save as a property of the window so it can be used by child dialogs

  window.editorShell = editorShell;
  
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

// THESE WILL BE REMOVE ONCE ALL DIALOGS ARE CONVERTED TO NEW WIDGETS
function AppendStringToListById(list, stringID)
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

function ValidateNumberString(value, minValue, maxValue)
{
  // Get the number version (strip out non-numbers)
  if (value.length > 0)
  {
    // Extract just numeric characters
    var number = Number(value.replace(/\D+/g, ""));
    if (number >= minValue && number <= maxValue )
    {
      // Return string version of the number
      return String(number);
    }
    var message = editorShell.GetString("ValidateNumber");
    // Replace variable placeholders in message with number values
    message = ((message.replace(/%n%/,number)).replace(/%min%/,minValue)).replace(/%max%/,maxValue);
    ShowInputErrorMessage(message);
  }
  // Return an empty string to indicate error
  return "";
}

function ShowInputErrorMessage(message)
{
  editorShell.AlertWithTitle(GetString("InputError"), message);
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
function SetElementEnabledById( elementID, doEnable )
{
  element = document.getElementById(elementID);
  if ( element )
  {
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
    dump("Element "+elementID+" not found in SetElementEnabledById\n");
  }
}

// This function relies on css classes for enabling and disabling
// This function takes an ID for a label and a flag
// if an element can be found by its ID, then it is either enabled or disabled
// The class is set to either "enabled" or "disabled" depending on the flag passed in.
// This function relies on css having a special appearance for these two classes.

function SetClassEnabledById( elementID, doEnable )
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
//  to determine what a "%" value is refering to.
// elementForAtt is element we are actually setting attributes on
//  (a temporary copy of element in the doc to allow canceling),
//  but elementInDoc is needed to find parent context in document
function GetAppropriatePercentString(elementForAtt, elementInDoc)
{
  if (elementForAtt.nodeName == "TD" || elementForAtt.nodeName == "TH")
    return GetString("PercentOfTable");

  // Check if element is within a table cell
  if(editorShell.GetElementOrParentByTagName("td",elementInDoc))
    return GetString("PercentOfCell");
  else
    return GetString("PercentOfWindow");
}

function InitPixelOrPercentMenulist(elementForAtt, elementInDoc, attribute, menulistID)
{
  var size  = elementForAtt.getAttribute(attribute);
  var menulist = document.getElementById(menulistID);
  var pixelItem;
  var percentItem;

  if (!menulist) 
  {
    dump("NO MENULIST found for ID="+menulistID+"\n");
    return size;
  }

  ClearMenulist(menulist);
  pixelItem = AppendStringToMenulist(menulist, GetString("Pixels"));
  percentItem = AppendStringToMenulist(menulist, GetAppropriatePercentString(elementForAtt, elementInDoc));

  // Search for a "%" character
  percentIndex = size.search(/%/);
  if (percentIndex > 0)
  {
    // Strip out the %
    size = size.substr(0, percentIndex);
    if (pixelItem)
      menulist.selectedItem = pixelItem;
  } 
  else if(percentItem)
    menulist.selectedItem = percentItem;

  return size;
}

function AppendStringToMenulistById(menulist, stringID)
{
  return AppendStringToMenulist(menulist, editorShell.GetString(stringID));
}

function AppendStringToMenulist(menulist, string)
{
  if (menulist)
  {
    var menupopup = menulist.firstChild;
    // May not have any popup yet -- so create one
    if (!menupopup)
    {
      menupopup = document.createElement("menupopup");
      if (menupopup)
        menulist.appendChild(menupopup);
      else
      {
dump("Failed to create menupoup\n");
        return null;
      }
    }
    menuItem = document.createElement("menuitem");
    if (menuItem)
    {
      menuItem.setAttribute("value", string);
      menupopup.appendChild(menuItem);
dump("AppendStringToMenulist, menuItem="+menuItem+", value="+string+"\n");
      return menuItem;
    }
  }
  return null;
}

function ClearMenulist(menulist)
{
  // There is usually not more than 1 menupopup under a menulist,
  //  but look for > 1 children anyway.
  // Note -- this doesn't remove menuitems from the menupopop -- SHOULD WE?
  if (menulist) {
dump(menulist+"=menulist in ClearMenulist\n");
    menulist.selectedItem = null;
    while (menulist.firstChild)
      menulist.removeChild(menulist.firstChild);
  }
}

/* These help using a <tree> for simple lists
  Assumes this simple structure:
  <tree>
    <treechildren>
      <treeitem>
        <treerow>
          <treecell value="the text the user sees"/>
*/

function AppendStringToTreelistById(tree, stringID)
{
  return AppendStringToTreelist(tree, editorShell.GetString(stringID));
}

function AppendStringToTreelist(tree, string)
{
  if (tree)
  {
    var treechildren = tree.firstChild;
    if (!treechildren)
    {
      treechildren = document.createElement("treechildren");
      if (treechildren)
        tree.appendChild(treechildren);
      else
      {
dump("Failed to create <treechildren>\n");
        return null;
      }
    }
    var treeitem = document.createElement("treeitem");
    var treerow = document.createElement("treerow");
    var treecell = document.createElement("treecell");
    if (treeitem && treerow && treecell)
    {
      treerow.appendChild(treecell);
      treeitem.appendChild(treerow);
      treechildren.appendChild(treeitem)
      treecell.setAttribute("value", string);
      var len = Number(tree.getAttribute("length"));
      if (!len) len = -1;
      tree.setAttribute("length",len+1);
      return treeitem;
    }
  }
  return null;
}

function ClearTreelist(tree)
{
  if (tree)
  {
    while (tree.firstChild)
      tree.removeChild(tree.firstChild);
    
    // Count list items
    tree.setAttribute("length", 0);
  }
}

function GetSelectedTreelistValue(tree)
{
  var treeCell = tree.selectedCell;
  if (treeCell)
    return treeCell.getAttribute("value");
  
  return ""; 
}

function SetSelectedTreelistItem()
{
}

// USE onkeyup!
// forceInteger by petejc (pete@postpagan.com)

// No one likes the beep!
//var sysBeep = Components.classes["component://netscape/sound"].createInstance();
//sysBeep     = sysBeep.QueryInterface(Components.interfaces.nsISound);

function forceInteger(elementID)
{
  var editField = document.getElementById( elementID );
  if ( !editField )
    return;

  var stringIn = editField.value;
  if (stringIn && stringIn.length > 0)
  {
    stringIn = stringIn.replace(/\D+/g,"");
    if (!stringIn) stringIn = "";
    // Strip out all nonnumeric characters
    editField.value = stringIn;

    // we hope to remove the following line for blur() once xp widgets land
    // cmanske (9/15) testing this now that GFX ender widget is active
    //editField.blur();
    //sysBeep.Beep();
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
    window.openDialog("chrome://editor/content/EdAdvancedEdit.xul", "_blank", "chrome,close,titlebar,modal,resizable=yes", "", globalElement);
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

function getColor(ColorPickerID)
{
  var colorPicker = document.getElementById(ColorPickerID);
  var color;
  if (colorPicker) 
  {
    // Extract color from colorPicker and assign to colorWell.
    color = colorPicker.getAttribute("color");
    if (color && color == "")
      return null;
    // Clear color so next if it's called again before
    //  color picker is actually used, we dedect the "don't set color" state
    colorPicker.setAttribute("color","");
  }

  return color;
}

function setColorWell(ColorWellID, color)
{
  var colorWell = document.getElementById(ColorWellID);
  if (colorWell)
  {
    if (!color || color == "")
    {
      // Don't set color (use default)
      // Trigger change to not show color swatch
      colorWell.setAttribute("default","true");
      // Style in CSS sets "background-color",
      //   but color won't clear unless we do this:
      colorWell.removeAttribute("style");
    }
    else
    {
      colorWell.removeAttribute("default");
      // Use setAttribute so colorwell can be a XUL element, such as titledbutton
      colorWell.setAttribute("style", "background-color:"+color);
    }
  }
}

function getColorAndSetColorWell(ColorPickerID, ColorWellID)
{
  var color = getColor(ColorPickerID);
  setColorWell(ColorWellID, color);
  return color;
}

// Test for valid image by sniffing out the extension
function IsValidImage(imageName)
{
  image = imageName.trimString();
  if ( !image )
    return false;
  
  /* look for an extension */
  var tailindex = image.lastIndexOf("."); 
  if ( tailindex == 0 || tailindex == -1 ) /* -1 is not found */
    return false; 
  
  /* move past period, get the substring from the first character after the '.' to the last character (length) */
  tailindex = tailindex + 1;
  var type = image.substring(tailindex,image.length);
  
  /* convert extension to lower case */
  if (type)
    type = type.toLowerCase();
  
  // TODO: Will we convert .BMPs to a web format?
  switch( type )  {
    case "gif":
    case "jpg":
    case "jpeg":
    case "png":
      return true;
      break;
    default :
     return false;
  }
}

function InitMoreFewer()
{
  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"' 
  //   attribute on the dialog.MoreFewerButton button
  //   onMoreFewer will toggle it and redraw the dialog
  SeeMore = (dialog.MoreFewerButton.getAttribute("more") != "1");
dump(SeeMore+"=SeeMore - InitMoreFewer\n");
  onMoreFewer();
}

function onMoreFewer()
{
  if (SeeMore)
  {
    dialog.MoreSection.setAttribute("style","display: none");
    //TODO: Bugs in box layout prevent us from using this:
    //dialog.MoreSection.setAttribute("style","visibility: collapse");
    window.sizeToContent();
    dialog.MoreFewerButton.setAttribute("more","0");
    dialog.MoreFewerButton.setAttribute("value",GetString("MoreProperties"));
    SeeMore = false;
  }
  else
  {
    dialog.MoreSection.setAttribute("style","display: inherit");
    //dialog.MoreSection.setAttribute("style","visibility: inherit");
    window.sizeToContent();
    dialog.MoreFewerButton.setAttribute("more","1");
    dialog.MoreFewerButton.setAttribute("value",GetString("FewerProperties"));
    SeeMore = true;
  }
}

function GetPrefs()
{
  var prefs;
  try {
    prefs = Components.classes['component://netscape/preferences'];
    if (prefs) prefs = prefs.getService();
    if (prefs) prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
    if (prefs)
      return prefs;
    else
      dump("failed to get prefs service!\n");

  }
  catch(ex)
  {
	  dump("failed to get prefs service!\n");
  }
  return null;
}

function GetScriptFileSpec()
{
  var fs = Components.classes["component://netscape/filespec"].createInstance();
  fs = fs.QueryInterface(Components.interfaces.nsIFileSpec);
  fs.unixStyleFilePath = "journal.js";
  return fs;
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function GetLocalFileURL(filterType)
{
  var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
  fp.init(window, editorShell.GetString("OpenHTMLFile"), nsIFilePicker.modeOpen);

  if (filterType == "img")
    fp.setFilters(nsIFilePicker.filterImages);
  else
    // While we allow "All", include filters that prefer HTML and Text files
    fp.setFilters(nsIFilePicker.filterText | nsIFilePicker.filterHTML | nsIFilePicker.filterAll);
  
  /* doesn't handle *.shtml files */
  try {
    fp.show();
    /* need to handle cancel (uncaught exception at present) */
  }
  catch (ex) {
    dump("filePicker.chooseInputFile threw an exception\n");
    return null;
  }
  
  // Convert native filepath to the URL format
  fs = GetScriptFileSpec();
  if (fs)
  {
dump(fp.file.path+" = native file path\n");
    fs.nativePath = fp.file.path;
dump(fs.URLString+" = URL string\n");
    return fs.URLString;
  }
  return null;
}
