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
 *   Ben "Count XULula" Goodger
 */

/**************       NAMESPACES       ***************/
const XUL_NS    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**************         GLOBALS         **************/
var gElement    = null; // handle to actual element edited

var HTMLAttrs   = [];   // html attributes
var CSSAttrs    = [];   // css attributes
var JSEAttrs    = [];   // js events

var HTMLRAttrs  = [];   // removed html attributes
var JSERAttrs   = [];   // removed js events

/* Set false to allow changing selection in tree
   without doing "onselect" handler actions
*/
var gDoOnSelectTree = true;
var gUpdateTreeValue = true;

var dialog;

/************** INITIALISATION && SETUP **************/

/**
 * function   : void Startup();
 * parameters : none
 * returns    : none
 * desc.      : startup and initialisation, prepares dialog.
 **/
function Startup()
{
  // This is the return value for the parent,
  // who only needs to know if OK was clicked
  window.opener.AdvancedEditOK = false;

  if (!InitEditorShell())
    return;

  // initialise the ok and cancel buttons
  doSetOKCancel(onOK, onCancel);

  // Element to edit is passed in
  if (!window.arguments[1])
  {
    dump("Advanced Edit: Element to edit not supplied\n");
    window.close();
    return;
  }
  // The actual element edited (not a copy!)
  gElement = window.arguments[1];

  // place the tag name in the header
  var tagLabel = document.getElementById("tagLabel");
  tagLabel.setAttribute("value", ("<" + gElement.localName + ">"));

  // Create dialog object to store controls for easy access
  dialog                            = {};
  dialog.AddHTMLAttributeNameInput  = document.getElementById("AddHTMLAttributeNameInput");

  // We use a <deck> to switch between editable menulist and textbox
  dialog.AddHTMLAttributeValueDeck     = document.getElementById("AddHTMLAttributeValueDeck");
  dialog.AddHTMLAttributeValueMenulist = document.getElementById("AddHTMLAttributeValueMenulist");
  dialog.AddHTMLAttributeValueTextbox  = document.getElementById("AddHTMLAttributeValueTextbox");
  dialog.AddHTMLAttributeValueInput    = dialog.AddHTMLAttributeValueTextbox;

  dialog.AddHTMLAttributeTree          = document.getElementById("HTMLATree");
  dialog.AddCSSAttributeNameInput      = document.getElementById("AddCSSAttributeNameInput");
  dialog.AddCSSAttributeValueInput     = document.getElementById("AddCSSAttributeValueInput");
  dialog.AddCSSAttributeTree           = document.getElementById("CSSATree");
  dialog.AddJSEAttributeNameList       = document.getElementById("AddJSEAttributeNameList");
  dialog.AddJSEAttributeValueInput     = document.getElementById("AddJSEAttributeValueInput");
  dialog.AddJSEAttributeTree           = document.getElementById("JSEATree");
  dialog.okButton                      = document.getElementById("ok");

  // build the attribute trees
  BuildHTMLAttributeTable();
  BuildCSSAttributeTable();
  BuildJSEAttributeTable();
  
  // Build attribute name arrays for menulists
  BuildJSEAttributeNameList();

  // No menulists for CSS panel (yet), so just clear input fields
  ClearCSSInputWidgets();

  // Do this last -- sets focus to HTML Name menulist
  BuildHTMLAttributeNameList();

  // size the dialog properly
  window.sizeToContent();

  SetWindowLocation();
}

/**
 * function   : bool onOK ( void );
 * parameters : none
 * returns    : boolean true to close the window
 * desc.      : event handler for ok button
 **/
function onOK()
{
  editorShell.BeginBatchChanges();
  try {
    // Update our gElement attributes
    UpdateHTMLAttributes();
    UpdateCSSAttributes();
    UpdateJSEAttributes();
  } catch(ex) {
    dump(ex);
  }
  editorShell.EndBatchChanges();

  window.opener.AdvancedEditOK = true;
  SaveWindowLocation();

  return true; // do close the window
}

// Helpers for removing and setting attributes
// Use editor transactions if modifying the element already in the document
// (Temporary element from a property dialog won't have a parent node)
function doRemoveAttribute(attrib)
{
  if (gElement.parentNode)
    editorShell.RemoveAttribute(gElement, attrib);
  else
    gElement.removeAttribute(attrib);
}

function doSetAttribute(attrib, value)
{
  if (gElement.parentNode)
    editorShell.SetAttribute(gElement, attrib, value);
  else
    gElement.setAttribute(attrib, value);
}

/**
 * function   : bool CheckAttributeNameSimilarity ( string attName, array attArray );
 * parameters : attribute to look for, array of current attributes
 * returns    : true if attribute already exists, false if it does not
 * desc.      : checks to see if any other attributes by the same name as the arg supplied
 *              already exist.
 **/
function CheckAttributeNameSimilarity(attName, attArray)
{
  for (var i = 0; i < attArray.length; i++)
  {
    if (attName.toLowerCase() == attArray[i].toLowerCase())
      return true;
  }
  return false;
}

/**
 * function   : bool UpdateExistingAttribute ( string attName, string attValue, string treeChildrenId );
 * parameters : attribute to look for, new value, ID of <treeChildren> node in XUL tree
 * returns    : true if attribute already exists in tree, false if it does not
 * desc.      : checks to see if any other attributes by the same name as the arg supplied
 *              already exist while setting the associated value if different from current value
 **/
function UpdateExistingAttribute( attName, attValue, treeChildrenId )
{
  var treeChildren = document.getElementById(treeChildrenId);
  if (!treeChildren)
    return false;

  var name;
  var i;
  attName = TrimString(attName).toLowerCase();
  attValue = TrimString(attValue);

  for (i = 0; i < treeChildren.childNodes.length; i++)
  {
    var item = treeChildren.childNodes[i];
    name = GetTreeItemAttributeStr(item);
    if (name.toLowerCase() == attName)
    {
      // Set the text in the "value' column treecell
      SetTreeItemValueStr(item, attValue);

      // Select item just changed, 
      //  but don't trigger the tree's onSelect handler
      gDoOnSelectTree = false;
      try {
        treeChildren.parentNode.selectItem(item);
      } catch (e) {}
      gDoOnSelectTree = true;

      return true;
    }
  }
  return false;
}

/**
 * function   : string GetAndSelectExistingAttributeValue ( string attName, string treeChildrenId );
 * parameters : attribute to look for, ID of <treeChildren> node in XUL tree
 * returns    : value in from the tree or empty string if name not found
 **/
function GetAndSelectExistingAttributeValue( attName, treeChildrenId )
{
  if (!attName)
    return "";

  var treeChildren = document.getElementById(treeChildrenId);
  var name;
  var i;

  for (i = 0; i < treeChildren.childNodes.length; i++)
  {
    var item = treeChildren.childNodes[i];
    name = GetTreeItemAttributeStr(item);
    if (name.toLowerCase() == attName.toLowerCase())
    {
      // Select item in the tree
      //  but don't trigger the tree's onSelect handler
      gDoOnSelectTree = false;
      try {
        treeChildren.parentNode.selectItem(item);
      } catch (e) {}
      gDoOnSelectTree = true;

      // Get the text in the "value' column treecell
      return GetTreeItemValueStr(item);
    }
  }

  // Attribute doesn't exist in tree, so remove selection
  gDoOnSelectTree = false;
  try {
    treeChildren.parentNode.clearItemSelection();
  } catch (e) {}
  gDoOnSelectTree = true;

  return "";
}

/* Tree structure: 
  <treeItem>
    <treeRow>
      <treeCell> // Name Cell
      <treeCell  // Value Cell
*/
function GetTreeItemAttributeStr(treeItem)
{
  if (treeItem)
    return TrimString(treeItem.firstChild.firstChild.getAttribute("label"));

  return "";
}

function GetTreeItemValueStr(treeItem)
{
  if (treeItem)
    return TrimString(treeItem.firstChild.lastChild.getAttribute("label"));

  return "";
}

function SetTreeItemValueStr(treeItem, value)
{
  if (treeItem && GetTreeItemValueStr(treeItem) != value)
    treeItem.firstChild.lastChild.setAttribute("label", value);
}

function IsNotTreeHeader(treeCell)
{
  if (treeCell)
    return (treeCell.parentNode.parentNode.nodeName != "treehead");

  return false;
}

function RemoveNameFromAttArray(attName, attArray)
{
  for (var i=0; i < attArray.length; i++)
  {
    if (attName.toLowerCase() == attArray[i].toLowerCase())
    {
      // Remove 1 array item
      attArray.splice(i,1);
      break;
    }
  }
}

// adds a generalised treeitem.
function AddTreeItem ( name, value, treeChildrenId, attArray )
{
  attArray[attArray.length] = name;
  var treeChildren    = document.getElementById ( treeChildrenId );
  var treeitem    = document.createElementNS ( XUL_NS, "treeitem" );
  var treerow     = document.createElementNS ( XUL_NS, "treerow" );

  var attrCell    = document.createElementNS ( XUL_NS, "treecell" );
  attrCell.setAttribute( "class", "propertylist" );
  attrCell.setAttribute( "label", name );

  var valueCell    = document.createElementNS ( XUL_NS, "treecell" );
  valueCell.setAttribute( "class", "propertylist" );
  valueCell.setAttribute( "label", value );

  treerow.appendChild ( attrCell );
  treerow.appendChild ( valueCell );
  treeitem.appendChild ( treerow );
  treeChildren.appendChild ( treeitem );

  // Select item just added,
  //  but suppress calling the onSelect handler
  gDoOnSelectTree = false;
  try {
    treeChildren.parentNode.selectItem(treeitem);
  } catch (e) {}
  gDoOnSelectTree = true;

  return treeitem;
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?advanced_property_editor");
}
