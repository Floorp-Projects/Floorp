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
 *   Charles Manske (cmanske@netscape.com)
 *   Neil Rashbrook (neil@parkwaycc.co.uk)
 */

// Each editor window must include this file

// Object to attach commonly-used widgets (all dialogs should use this)
var gDialog = {};

var gValidationError = false;

// Use for 'defaultIndex' param in InitPixelOrPercentMenulist
const gPixel = 0;
const gPercent = 1;

const gMaxPixels  = 100000; // Used for image size, borders, spacing, and padding
// Gecko code uses 1000 for maximum rowspan, colspan
// Also, editing performance is really bad above this
const gMaxRows    = 1000;
const gMaxColumns = 1000;
const gMaxTableSize = 1000000; // Width or height of table or cells

// For dialogs that expand in size. Default is smaller size see "onMoreFewer()" below
var SeeMore = false;

// A XUL element with id="location" for managing
// dialog location relative to parent window
var gLocation;

// The element being edited - so AdvancedEdit can have access to it
var globalElement;

/* Validate contents of an input field 
 *
 *  inputWidget    The 'textbox' XUL element for text input of the attribute's value
 *  listWidget     The 'menulist' XUL element for choosing "pixel" or "percent"
 *                  May be null when no pixel/percent is used.
 *  minVal         minimum allowed for input widget's value
 *  maxVal         maximum allowed for input widget's value
 *                 (when "listWidget" is used, maxVal is used for "pixel" maximum,
 *                  100% is assumed if "percent" is the user's choice)
 *  element        The DOM element that we set the attribute on. May be null.
 *  attName        Name of the attribute to set.  May be null or ignored if "element" is null
 *  mustHaveValue  If true, error dialog is displayed if "value" is empty string
 *
 *  This calls "ValidateNumberRange()", which puts up an error dialog to inform the user. 
 *    If error, we also: 
 *      Shift focus and select contents of the inputWidget,
 *      Switch to appropriate panel of tabbed dialog if user implements "SwitchToValidate()",
 *      and/or will expand the dialog to full size if "More / Fewer" feature is implemented
 *
 *  Returns the "value" as a string, or "" if error or input contents are empty
 *  The global "gValidationError" variable is set true if error was found
 */
function ValidateNumber(inputWidget, listWidget, minVal, maxVal, element, attName, mustHaveValue, mustShowMoreSection)
{
  if (!inputWidget)
  {
    gValidationError = true;
    return "";
  }

  // Global error return value
  gValidationError = false;
  var maxLimit = maxVal;
  var isPercent = false;

  var numString = TrimString(inputWidget.value);
  if (numString || mustHaveValue)
  {
    if (listWidget)
      isPercent = (listWidget.selectedIndex == 1);
    if (isPercent)
      maxLimit = 100;

    // This method puts up the error message
    numString = ValidateNumberRange(numString, minVal, maxLimit, mustHaveValue);
    if(!numString)
    {
      // Switch to appropriate panel for error reporting
      SwitchToValidatePanel();

      // or expand dialog for users of "More / Fewer" button
      if ("dialog" in window && dialog && 
           "MoreSection" in gDialog && gDialog.MoreSection)
      {
        if ( !SeeMore )
          onMoreFewer();
      }

      // Error - shift to offending input widget
      SetTextboxFocus(inputWidget);
      gValidationError = true;
    }
    else
    {
      if (isPercent)
        numString += "%";
      if (element)
        element.setAttribute(attName, numString);
    }
  } else if (element) {
    GetCurrentEditor().removeAttributeOrEquivalent(element, attName, true)
  }
  return numString;
}

/* Validate contents of an input field 
 *
 *  value          number to validate
 *  minVal         minimum allowed for input widget's value
 *  maxVal         maximum allowed for input widget's value
 *                 (when "listWidget" is used, maxVal is used for "pixel" maximum,
 *                  100% is assumed if "percent" is the user's choice)
 *  mustHaveValue  If true, error dialog is displayed if "value" is empty string
 *
 *  If inputWidget's value is outside of range, or is empty when "mustHaveValue" = true,
 *      an error dialog is popuped up to inform the user. The focus is shifted
 *      to the inputWidget.
 *
 *  Returns the "value" as a string, or "" if error or input contents are empty
 *  The global "gValidationError" variable is set true if error was found
 */
function ValidateNumberRange(value, minValue, maxValue, mustHaveValue)
{
  // Initialize global error flag
  gValidationError = false;
  value = TrimString(String(value));

  // We don't show error for empty string unless caller wants to
  if (!value && !mustHaveValue)
    return "";

  var numberStr = "";

  if (value.length > 0)
  {
    // Extract just numeric characters
    var number = Number(value.replace(/\D+/g, ""));
    if (number >= minValue && number <= maxValue )
    {
      // Return string version of the number
      return String(number);
    }
    numberStr = String(number);
  }

  var message = "";

  if (numberStr.length > 0)
  {
    // We have a number from user outside of allowed range
    message = GetString( "ValidateRangeMsg");
    message = message.replace(/%n%/, numberStr);
    message += "\n ";
  }
  message += GetString( "ValidateNumberMsg");

  // Replace variable placeholders in message with number values
  message = message.replace(/%min%/, minValue).replace(/%max%/, maxValue);
  ShowInputErrorMessage(message);

  // Return an empty string to indicate error
  gValidationError = true;
  return "";
}

function SetTextboxFocusById(id)
{
  SetTextboxFocus(document.getElementById(id));
}

function SetTextboxFocus(textbox)
{
  if (textbox)
  {
    //XXX Using the setTimeout is hacky workaround for bug 103197
    // Must create a new function to keep "textbox" in scope
    setTimeout( function(textbox) { textbox.focus(); textbox.select(); }, 0, textbox );
  }
}

function ShowInputErrorMessage(message)
{
  AlertWithTitle(GetString("InputError"), message);
  window.focus();
}

// Get the text appropriate to parent container
//  to determine what a "%" value is referring to.
// elementForAtt is element we are actually setting attributes on
//  (a temporary copy of element in the doc to allow canceling),
//  but elementInDoc is needed to find parent context in document
function GetAppropriatePercentString(elementForAtt, elementInDoc)
{
  var editor = GetCurrentEditor();
  try {
    var name = elementForAtt.nodeName.toLowerCase();
    if ( name == "td" || name == "th")
      return GetString("PercentOfTable");

    // Check if element is within a table cell
    if (editor.getElementOrParentByTagName("td", elementInDoc))
      return GetString("PercentOfCell");
    else
      return GetString("PercentOfWindow");
  } catch (e) { return "";}
}

function ClearListbox(listbox)
{
  if (listbox)
  {
    listbox.clearSelection();
    while (listbox.firstChild)
      listbox.removeChild(listbox.firstChild);
  }
}

function forceInteger(elementID)
{
  var editField = document.getElementById( elementID );
  if ( !editField )
    return;

  var stringIn = editField.value;
  if (stringIn && stringIn.length > 0)
  {
    // Strip out all nonnumeric characters
    stringIn = stringIn.replace(/\D+/g,"");
    if (!stringIn) stringIn = "";

    // Write back only if changed
    if (stringIn != editField.value)
      editField.value = stringIn;
  }
}

function LimitStringLength(elementID, length)
{
  var editField = document.getElementById( elementID );
  if ( !editField )
    return;

  var stringIn = editField.value;
  if (stringIn && stringIn.length > length)
    editField.value = stringIn.slice(0,length);
}

function InitPixelOrPercentMenulist(elementForAtt, elementInDoc, attribute, menulistID, defaultIndex)
{
  if (!defaultIndex) defaultIndex = gPixel;

  // var size  = elementForAtt.getAttribute(attribute);
  var size = GetHTMLOrCSSStyleValue(elementForAtt, attribute, attribute)
  var menulist = document.getElementById(menulistID);
  var pixelItem;
  var percentItem;

  if (!menulist)
  {
    dump("NO MENULIST found for ID="+menulistID+"\n");
    return size;
  }

  menulist.removeAllItems();
  pixelItem = menulist.appendItem(GetString("Pixels"));

  if (!pixelItem) return 0;

  percentItem = menulist.appendItem(GetAppropriatePercentString(elementForAtt, elementInDoc));
  if (size && size.length > 0)
  {
    // Search for a "%" or "px"
    if (/%/.test(size))
    {
      // Strip out the %
      size = RegExp.leftContext;
      if (percentItem)
        menulist.selectedItem = percentItem;
    }
    else
    {
      if (/px/.test(size))
        // Strip out the px
        size = RegExp.leftContext;
      menulist.selectedItem = pixelItem;
    }
  }
  else
    menulist.selectedIndex = defaultIndex;

  return size;
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
    window.focus();
    if (window.AdvancedEditOK)
    {
      // Copy edited attributes to the dialog widgets:
      InitDialog();
    }
  }
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
      // Use setAttribute so colorwell can be a XUL element, such as button
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

function InitMoreFewer()
{
  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"'
  //   attribute on the gDialog.MoreFewerButton button
  //   onMoreFewer will toggle it and redraw the dialog
  SeeMore = (gDialog.MoreFewerButton.getAttribute("more") != "1");
  onMoreFewer();
  gDialog.MoreFewerButton.setAttribute("accesskey",GetString("PropertiesAccessKey"));
}

function onMoreFewer()
{
  if (SeeMore)
  {
    gDialog.MoreSection.collapsed = true;
    gDialog.MoreFewerButton.setAttribute("more","0");
    gDialog.MoreFewerButton.setAttribute("label",GetString("MoreProperties"));
    SeeMore = false;
  }
  else
  {
    gDialog.MoreSection.collapsed = false;
    gDialog.MoreFewerButton.setAttribute("more","1");
    gDialog.MoreFewerButton.setAttribute("label",GetString("FewerProperties"));
    SeeMore = true;
  }
  window.sizeToContent();
}

function SwitchToValidatePanel()
{
  // no default implementation
  // Only EdTableProps.js currently implements this
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function GetLocalFileURL(filterType)
{
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  var fileType = "html";

  if (filterType == "img")
  {
    fp.init(window, GetString("SelectImageFile"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterImages);
    fileType = "image";
  }
  // Current usage of this is in Link dialog,
  //  where we always want HTML first
  else if (filterType.indexOf("html") == 0)
  {
    fp.init(window, GetString("OpenHTMLFile"), nsIFilePicker.modeOpen);

    // When loading into Composer, direct user to prefer HTML files and text files,
    //   so we call separately to control the order of the filter list
    fp.appendFilters(nsIFilePicker.filterHTML);
    fp.appendFilters(nsIFilePicker.filterText);

    // Link dialog also allows linking to images
    if (filterType.indexOf("img") > 0)
      fp.appendFilters(nsIFilePicker.filterImages);

  }
  // Default or last filter is "All Files"
  fp.appendFilters(nsIFilePicker.filterAll);

  // set the file picker's current directory to last-opened location saved in prefs
  SetFilePickerDirectory(fp, fileType);


  /* doesn't handle *.shtml files */
  try {
    var ret = fp.show();
    if (ret == nsIFilePicker.returnCancel)
      return null;
  }
  catch (ex) {
    dump("filePicker.chooseInputFile threw an exception\n");
    return null;
  }
  SaveFilePickerDirectory(fp, fileType);
  
  var fileHandler = GetFileProtocolHandler();
  return fp.file ? fileHandler.getURLSpecFromFile(fp.file) : null;
}

function GetMetaElement(name)
{
  if (name)
  {
    name = name.toLowerCase();
    if (name != "")
    {
      var editor = GetCurrentEditor();
      try {
        var metaNodes = editor.document.getElementsByTagName("meta");
        for (var i = 0; i < metaNodes.length; i++)
        {
          var metaNode = metaNodes.item(i);
          if (metaNode && metaNode.getAttribute("name") == name)
            return metaNode;
        }
      } catch (e) {}
    }
  }
  return null;
}

function CreateMetaElement(name)
{
  var editor = GetCurrentEditor();
  try {
    var metaElement = editor.createElementWithDefaults("meta");
    metaElement.setAttribute("name", name);
    return metaElement;
  } catch (e) {}

  return null;
}

function GetHTTPEquivMetaElement(name)
{
  if (name)
  {
    name = name.toLowerCase();
    if (name != "")
    {
      var editor = GetCurrentEditor();
      try {
        var metaNodes = editor.document.getElementsByTagName("meta");
        for (var i = 0; i < metaNodes.length; i++)
        {
          var metaNode = metaNodes.item(i);
          if (metaNode)
          {
            var httpEquiv = metaNode.getAttribute("http-equiv");
            if (httpEquiv && httpEquiv.toLowerCase() == name)
              return metaNode;
          }
        }
      } catch (e) {}
    }
  }
  return null;
}

function CreateHTTPEquivMetaElement(name)
{
  var editor = GetCurrentEditor();
  try {
    var metaElement = editor.createElementWithDefaults("meta");
    metaElement.setAttribute("http-equiv", name);
    return metaElement;
  } catch (e) {}

  return null;
}

function CreateHTTPEquivElement(name)
{
  var editor = GetCurrentEditor();
  try {
    var metaElement = editor.createElementWithDefaults("meta");
    metaElement.setAttribute("http-equiv", name);
    return metaElement;
  } catch (e) {}

  return null;
}

// Change "content" attribute on a META element,
//   or delete entire element it if content is empty
// This uses undoable editor transactions
function SetMetaElementContent(metaElement, content, insertNew, prepend)
{
  if (metaElement)
  {
    var editor = GetCurrentEditor();
    try {
      if(!content || content == "")
      {
        if (!insertNew)
          editor.deleteNode(metaElement);
      }
      else
      {
        if (insertNew)
        {
          metaElement.setAttribute("content", content);
          if (prepend)
            PrependHeadElement(metaElement);
          else
            AppendHeadElement(metaElement);
        }
        else
          editor.setAttribute(metaElement, "content", content);
      }
    } catch (e) {}
  }
}

function GetHeadElement()
{
  var editor = GetCurrentEditor();
  try {
    var headList = editor.document.getElementsByTagName("head");
    return headList.item(0);
  } catch (e) {}

  return null;
}

function PrependHeadElement(element)
{
  var head = GetHeadElement();
  if (head)
  {
    var editor = GetCurrentEditor();
    try {
      // Use editor's undoable transaction
      // Last param "true" says "don't change the selection"
      editor.insertNode(element, head, 0, true);
    } catch (e) {}
  }
}

function AppendHeadElement(element)
{
  var head = GetHeadElement();
  if (head)
  {
    var position = 0;
    if (head.hasChildNodes())
      position = head.childNodes.length;

    var editor = GetCurrentEditor();
    try {
      // Use editor's undoable transaction
      // Last param "true" says "don't change the selection"
      editor.insertNode(element, head, position, true);
    } catch (e) {}
  }
}

function SetWindowLocation()
{
  gLocation = document.getElementById("location");
  if (gLocation)
  {
    window.screenX = Math.max(0, Math.min(window.opener.screenX + Number(gLocation.getAttribute("offsetX")),
                                          screen.availWidth - window.outerWidth));
    window.screenY = Math.max(0, Math.min(window.opener.screenY + Number(gLocation.getAttribute("offsetY")),
                                          screen.availHeight - window.outerHeight));
  }
}

function SaveWindowLocation()
{
  if (gLocation)
  {
    var newOffsetX = window.screenX - window.opener.screenX;
    var newOffsetY = window.screenY - window.opener.screenY;
    gLocation.setAttribute("offsetX", window.screenX - window.opener.screenX);
    gLocation.setAttribute("offsetY", window.screenY - window.opener.screenY);
  }
}

function onCancel()
{
  SaveWindowLocation();
  // Close dialog by returning true
  return true;
}

function SetRelativeCheckbox(checkbox)
{
  if (!checkbox) {
    checkbox = document.getElementById("MakeRelativeCheckbox");
    if (!checkbox)
      return;
  }

  var editor = GetCurrentEditor();
  // Mail never allows relative URLs, so hide the checkbox
  if (editor && (editor.flags & Components.interfaces.nsIPlaintextEditor.eEditorMailMask))
  {
    checkbox.collapsed = true;
    return;
  }

  var input =  document.getElementById(checkbox.getAttribute("for"));
  if (!input)
    return;

  var url = TrimString(input.value);
  var urlScheme = GetScheme(url);

  // Check it if url is relative (no scheme).
  checkbox.checked = url.length > 0 && !urlScheme;

  // Now do checkbox enabling:
  var enable = false;

  var docUrl = GetDocumentBaseUrl();
  var docScheme = GetScheme(docUrl);

  if (url && docUrl && docScheme)
  {
    if (urlScheme)
    {
      // Url is absolute
      // If we can make a relative URL, then enable must be true!
      // (this lets the smarts of MakeRelativeUrl do all the hard work)
      enable = (GetScheme(MakeRelativeUrl(url)).length == 0);
    }
    else
    {
      // Url is relative
      // Check if url is a named anchor
      //  but document doesn't have a filename
      // (it's probably "index.html" or "index.htm",
      //  but we don't want to allow a malformed URL)
      if (url[0] == "#")
      {
        var docFilename = GetFilename(docUrl);
        enable = docFilename.length > 0;
      }
      else
      {
        // Any other url is assumed 
        //  to be ok to try to make absolute
        enable = true;
      }
    }
  }

  SetElementEnabled(checkbox, enable);
}

// oncommand handler for the Relativize checkbox in EditorOverlay.xul
function MakeInputValueRelativeOrAbsolute(checkbox)
{
  var input =  document.getElementById(checkbox.getAttribute("for"));
  if (!input)
    return;

  var docUrl = GetDocumentBaseUrl();
  if (!docUrl)
  {
    // Checkbox should be disabled if not saved,
    //  but keep this error message in case we change that
    AlertWithTitle("", GetString("SaveToUseRelativeUrl"));
    window.focus();
  }
  else 
  {
    // Note that "checked" is opposite of its last state,
    //  which determines what we want to do here
    if (checkbox.checked)
      input.value = MakeRelativeUrl(input.value);
    else
      input.value = MakeAbsoluteUrl(input.value);

    // Reset checkbox to reflect url state
    SetRelativeCheckbox(checkbox);
  }
}

var IsBlockParent = {
  APPLET: true,
  BLOCKQUOTE: true,
  BODY: true,
  CENTER: true,
  DD: true,
  DIV: true,
  FORM: true,
  LI: true,
  NOSCRIPT: true,
  OBJECT: true,
  TD: true,
  TH: true
};

var NotAnInlineParent = {
  COL: true,
  COLGROUP: true,
  DL: true,
  DIR: true,
  MENU: true,
  OL: true,
  TABLE: true,
  TBODY: true,
  TFOOT: true,
  THEAD: true,
  TR: true,
  UL: true
};

function nodeIsBreak(editor, node)
{
  return !node || node.localName == 'BR' || editor.nodeIsBlock(node);
}

function InsertElementAroundSelection(element)
{
  var editor = GetCurrentEditor();
  editor.beginTransaction();

  try {
    // First get the selection as a single range
    var range, start, end, offset;
    var count = editor.selection.rangeCount;
    if (count == 1)
      range = editor.selection.getRangeAt(0).cloneRange();
    else
    {
      range = editor.document.createRange();
      start = editor.selection.getRangeAt(0)
      range.setStart(start.startContainer, start.startOffset);
      end = editor.selection.getRangeAt(--count);
      range.setEnd(end.endContainer, end.endOffset);
    }

    // Flatten the selection to child nodes of the common ancestor
    while (range.startContainer != range.commonAncestorContainer)
      range.setStartBefore(range.startContainer);
    while (range.endContainer != range.commonAncestorContainer)
      range.setEndAfter(range.endContainer);

    if (editor.nodeIsBlock(element))
      // Block element parent must be a valid block
      while (!(range.commonAncestorContainer.localName in IsBlockParent))
        range.selectNode(range.commonAncestorContainer);
    else
    {
      // Fail if we're not inserting a block (use setInlineProperty instead)
      if (!nodeIsBreak(editor, range.commonAncestorContainer))
        return false;
      else if (range.commonAncestorContainer.localName in NotAnInlineParent)
        // Inline element parent must not be an invalid block
        do range.selectNode(range.commonAncestorContainer);
        while (range.commonAncestorContainer.localName in NotAnInlineParent);
      else
        // Further insert block check
        for (var i = range.startOffset; ; i++)
          if (i == range.endOffset)
            return false;
          else if (nodeIsBreak(editor, range.commonAncestorContainer.childNodes[i]))
            break;
    }

    // The range may be contained by body text, which should all be selected.
    offset = range.startOffset;
    start = range.startContainer.childNodes[offset];
    if (!nodeIsBreak(editor, start))
    {
      while (!nodeIsBreak(editor, start.previousSibling))
      {
        start = start.previousSibling;
        offset--;
      }
    }
    end = range.endContainer.childNodes[range.endOffset];
    if (end && !nodeIsBreak(editor, end.previousSibling))
    {
      while (!nodeIsBreak(editor, end))
        end = end.nextSibling;
    }

    // Now insert the node
    editor.insertNode(element, range.commonAncestorContainer, offset, true);
    offset = element.childNodes.length;
    if (!editor.nodeIsBlock(element))
      editor.setShouldTxnSetSelection(false);

    // Move all the old child nodes to the element
    var empty = true;
    while (start != end)
    {
      var next = start.nextSibling;
      editor.deleteNode(start);
      editor.insertNode(start, element, element.childNodes.length);
      empty = false;
      start = next;
    }
    if (!editor.nodeIsBlock(element))
      editor.setShouldTxnSetSelection(true);
    else
    {
      // Also move a trailing <br>
      if (start && start.localName == 'BR')
      {
        editor.deleteNode(start);
        editor.insertNode(start, element, element.childNodes.length);
        empty = false;
      }
      // Still nothing? Insert a <br> so the node is not empty
      if (empty)
        editor.insertNode(editor.createElementWithDefaults("br"), element, element.childNodes.length);

      // Hack to set the selection just inside the element
      editor.insertNode(editor.document.createTextNode(""), element, offset);
    }
  }
  finally {
    editor.endTransaction();
  }

  return true;
}

function nodeIsBlank(node)
{
  return node && node.NODE_TYPE == Node.TEXT_NODE && !/\S/.test(node.data);
}

function nodeBeginsBlock(node)
{
  while (nodeIsBlank(node))
    node = node.nextSibling;
  return nodeIsBlock(node);
}

function nodeEndsBlock(node)
{
  while (nodeIsBlank(node))
    node = node.previousSibling;
  return nodeIsBlock(node);
}

// C++ function isn't exposed to JS :-(
function RemoveBlockContainer(element)
{
  var editor = GetCurrentEditor();
  editor.beginTransaction();

  try {
    var range = editor.document.createRange();
    range.selectNode(element);
    var offset = range.startOffset;
    var parent = element.parentNode;

    // May need to insert a break after the removed element
    if (!nodeBeginsBlock(editor, element.nextSibling) &&
        !nodeEndsBlock(editor, element.lastChild))
      editor.insertNode(editor.createElementWithDefaults("br"), parent, range.endOffset);

    // May need to insert a break before the removed element, or if it was empty
    if (!nodeEndsBlock(editor, element.previousSibling) &&
        !nodeBeginsBlock(editor, element.firstChild || element.nextSibling))
      editor.insertNode(editor.createElementWithDefaults("br"), parent, offset++);

    // Now remove the element
    editor.deleteNode(element);

    // Need to copy the contained nodes?
    for (var i = 0; i < element.childNodes.length; i++)
      editor.insertNode(element.childNodes[i].cloneNode(true), parent, offset++);
  }
  finally {
    editor.endTransaction();
  }
}

// C++ function isn't exposed to JS :-(
function RemoveContainer(element)
{
  var editor = GetCurrentEditor();
  editor.beginTransaction();

  try {
    var range = editor.document.createRange();
    var parent = element.parentNode;
    // Allow for automatic joining of text nodes
    // so we can't delete the container yet
    // so we need to copy the contained nodes
    for (var i = 0; i < element.childNodes.length; i++) {
      range.selectNode(element);
      editor.insertNode(element.childNodes[i].cloneNode(true), parent, range.startOffset);
    }
    // Now remove the element
    editor.deleteNode(element);
  }
  finally {
    editor.endTransaction();
  }
}

function FillLinkMenulist(linkMenulist, headingsArray)
{
  var menupopup = linkMenulist.firstChild;
  var editor = GetCurrentEditor();
  try {
    var treeWalker = editor.document.createTreeWalker(editor.document, 1, null, true);
    var headingList = [];
    var anchorList = []; // for sorting
    var anchorMap  = {}; // for weeding out duplicates and making heading anchors unique
    var anchor;
    var i;
    for (var element = treeWalker.nextNode(); element; element = treeWalker.nextNode())
    {
      // grab headings
      // Skip headings that already have a named anchor as their first child
      //  (this may miss nearby anchors, but at least we don't insert another
      //   under the same heading)
      if (element instanceof HTMLHeadingElement && element.textContent &&
          !(element.firstChild instanceof HTMLAnchorElement && element.firstChild.name))
        headingList.push(element);

      // grab named anchors
      if (element instanceof HTMLAnchorElement && element.name)
      {
        anchor = '#' + element.name;
        if (!(anchor in anchorMap))
        {
          anchorList.push({anchor: anchor, sortkey: anchor.toLowerCase()});
          anchorMap[anchor] = true;
        }
      }
      
      // grab IDs
      if (element.id)
      {
        anchor = '#' + element.id;
        if (!(anchor in anchorMap))
        {
          anchorList.push({anchor: anchor, sortkey: anchor.toLowerCase()});
          anchorMap[anchor] = true;
        }
      }
    }
    // add anchor for headings
    for (i = 0; i < headingList.length; i++)
    {
      var heading = headingList[i];

      // Use just first 40 characters, don't add "...",
      //  and replace whitespace with "_" and strip non-word characters
      anchor = '#' + ConvertToCDATAString(TruncateStringAtWordEnd(heading.textContent, 40, false));

      // Append "_" to any name already in the list
      while (anchor in anchorMap)
        anchor += "_";
      anchorList.push({anchor: anchor, sortkey: anchor.toLowerCase()});
      anchorMap[anchor] = true;

      // Save nodes in an array so we can create anchor node under it later
      headingsArray[anchor] = heading;
    }
    if (anchorList.length)
    {
      // case insensitive sort
      function compare(a, b)
      {
        if(a.sortkey < b.sortkey) return -1;
        if(a.sortkey > b.sortkey) return 1;
        return 0;
      }
      anchorList.sort(compare);

      for (i = 0; i < anchorList.length; i++)
        createMenuItem(menupopup,anchorList[i].anchor);
    }
    else
    {
      var item = createMenuItem(menupopup, GetString("NoNamedAnchorsOrHeadings"));
      item.setAttribute("disabled", "true");
    }
  } catch (e) {}
}

function createMenuItem(aMenuPopup, aLabel)
{
  var menuitem = document.createElement("menuitem");
  menuitem.setAttribute("label", aLabel);
  aMenuPopup.appendChild(menuitem);
  return menuitem;
}

// Shared by Image and Link dialogs for the "Choose" button for links
function chooseLinkFile()
{
  // Get a local file, converted into URL format
  var fileName = GetLocalFileURL("html, img");
  if (fileName) 
  {
    // Always try to relativize local file URLs
    if (gHaveDocumentUrl)
      fileName = MakeRelativeUrl(fileName);

    gDialog.hrefInput.value = fileName;

    // Do stuff specific to a particular dialog
    // (This is defined separately in Image and Link dialogs)
    ChangeLinkLocation();
  }
  // Put focus into the input field
  SetTextboxFocus(gDialog.hrefInput);
}

