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

/**************         FOR NOW         **************/
const TEXT_WIDGETS_DONT_SUCK  = false;
const PERFORMANCE_BOOSTS      = false;

/**************       NAMESPACES       ***************/
const XUL_NS    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
 
/**************         GLOBALS         **************/

var tagname     = null; // element.nodeName;
var element     = null; // handle for the actual element

var HTMLAttrs   = [];   // html attributes
var CSSAttrs    = [];   // css attributes
var JSEAttrs    = [];   // js events 

var gSelecting = false; // To prevent recursive selection
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
  element = window.arguments[1];
  if (!element || element == undefined) {
    dump("Advanced Edit: Element to edit not supplied\n");
    window.close();  
  }

  // place the tag name in the header
  var tagLabel = document.getElementById("tagLabel");
  tagLabel.setAttribute("value", ("<" + element.localName + ">"));

  // Create dialog object to store controls for easy access
  dialog                            = {};
  dialog.AddHTMLAttributeNameInput  = document.getElementById("AddHTMLAttributeNameInput");
  dialog.AddHTMLAttributeValueInput = document.getElementById("AddHTMLAttributeValueInput");
  dialog.AddHTMLAttribute           = document.getElementById("AddHTMLAttribute");
  dialog.AddCSSAttributeNameInput   = document.getElementById("AddCSSAttributeNameInput");
  dialog.AddCSSAttributeValueInput  = document.getElementById("AddCSSAttributeValueInput");
  dialog.AddCSSAttribute            = document.getElementById("AddCSSAttribute");
  dialog.AddJSEAttributeNameInput   = document.getElementById("AddJSEAttributeNameInput");
  dialog.AddJSEAttributeValueInput  = document.getElementById("AddJSEAttributeValueInput");
  dialog.AddJSEAttribute            = document.getElementById("AddJSEAttribute");

  // build the attribute trees
  BuildHTMLAttributeTable();
  BuildCSSAttributeTable();
  BuildJSEAttributeTable();
 
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
  UpdateObject(); // call UpdateObject fn to update element in document
  window.opener.AdvancedEditOK = true;
  window.opener.globalElement = element;

  SaveWindowLocation();

  return true; // do close the window
}

/**
 * function   : void UpdateObject ( void );
 * parameters : none
 * returns    : none
 * desc.      : Updates the copy of the page object with the data set in this dialog.
 **/
function UpdateObject()
{
    UpdateHTMLAttributes();
    UpdateCSSAttributes();
    UpdateJSEAttributes();
}

/**
 * function   : bool CheckAttributeNameSimilarity ( string attName, array attArray );
 * parameters : attribute to look for, array of current attributes
 * returns    : false if attribute already exists, true if it does not
 * desc.      : checks to see if any other attributes by the same name as the arg supplied
 *              already exist.
 **/
function CheckAttributeNameSimilarity(attName, attArray)
{
  for(var i = 0; i < attArray.length; i++)
  {
    if(attName == attArray[i]) 
      return false;
  }
  return true;
}

/**
 * function   : bool CheckAttributeNotRemoved ( string attName, array attArray );
 * parameters : attribute to look for, array of deleted attributes
 * returns    : false if attribute already exists, true if it does not
 * desc.      : check to see if the attribute is in the array marked for removal 
 *              before updating the final object
 **/
function CheckAttributeNotRemoved( attName, attArray )
{
  for( var i = 0; i < attArray.length; i++ ) 
  {
    if( attName == attArray[i] )
      return false;
  }
  return true;
}

/**
 * function   : void doRemoveAttribute( DOMElement which);
 * parameters : DOMElement that was context-clicked.
 * returns    : nothing
 * desc.      : removes an attribute or attributes from the tree
 **/
function RemoveAttribute( treeId )
{
  var tree = document.getElementById(treeId);
  if (!tree) return;
  
  var kids = tree.lastChild;  // treechildren element of tree
  var newIndex = tree.selectedIndex;
  // We only allow 1 selected item
  if (tree.selectedItems.length)
  {
    var item = tree.selectedItems[0];
    // Name is the value of the treecell
    var name = item.firstChild.firstChild.getAttribute("value");

    // remove the item from the attribute arrary
    switch ( tree.id ) {
      case "HTMLATree":
        if (newIndex >= (HTMLAttrs.length-1))
          newIndex--;
        RemoveNameFromAttArray(HTMLAttrs, name);
        break;
      case "CSSATree":   
        if (newIndex >= (CSSAttrs.length-1))
          newIndex--;
        RemoveNameFromAttArray(CSSAttrs, name);
        break;
      case "JSEATree":
        if (newIndex >= (JSEAttrs.length-1))
          newIndex--;
        RemoveNameFromAttArray(JSEAttrs, name);
        break;      
      default: break;
    }

    // Remove the item from the tree
    kids.removeChild (item);

    // Reselect an item
    tree.selectedIndex = newIndex;
    SelectTreeItem(tree);
  }
}
function RemoveNameFromAttArray(attArray, name)
{
  for (var i=0; i < attArray.length; i++)
  {
    if (attArray[i] == name)
    {
      // Remove 1 array item
      attArray.splice(i,1);
      break; 
    }
  }
}

// NOT USED
/*
function doSelect(e)
{
  if ( TEXT_WIDGETS_DONT_SUCK && PERFORMANCE_BOOSTS ) {
    var cell  = e.target.parentNode.lastChild;
    var input = cell.firstChild;

    var selitems = document.getElementsByAttribute("class","FocusSelected");
    for ( var i = 0; i < selitems.length; i++ )
    {
      if ( selitems[i].nodeName == "input" ) {
        selitems[i].removeAttribute("class","FocusSelected");
        selitems[i].setAttribute("class","AttributesCell");
      } else if ( selitems[i].nodeName == "treecell" )
        selitems[i].removeAttribute("class","FocusSelected");
    }
  
    cell.setAttribute("class","FocusSelected");
    input.setAttribute("class","FocusSelected");
    SetTextfieldFocus(input);
  }
}
*/

// adds a generalised treeitem.
function AddTreeItem ( name, value, treekidsId, attArray, valueCaseFunc )
{
  attArray[attArray.length] = name;
  var treekids    = document.getElementById ( treekidsId );
  var treeitem    = document.createElementNS ( XUL_NS, "treeitem" );
  var treerow     = document.createElementNS ( XUL_NS, "treerow" );
  var attrcell    = document.createElementNS ( XUL_NS, "treecell" );
  attrcell.setAttribute( "class", "propertylist" );
  attrcell.setAttribute( "value", name.toLowerCase() );
  // Modify treerow selection to better show focus in textfield
  treeitem.setAttribute( "class", "ae-selection");

  treerow.appendChild ( attrcell );
  
  if ( !valueCaseFunc ) {
    // no handling function provided, create default cell.
    var valCell = CreateCellWithField ( name, value );
    if (!valCell) return null;
    treerow.appendChild ( valCell );
  } else 
    valueCaseFunc();  // run user specified function for adding content
  
  treeitem.appendChild ( treerow );
  treekids.appendChild ( treeitem );
  return treeitem;
}

// creates a generic treecell with field inside.
// optional parameters for initial values.
function CreateCellWithField( name, value )
{
  var valCell     = document.createElementNS ( XUL_NS, "treecell" );
  if (!valCell) return null;
  valCell.setAttribute ( "class", "value propertylist" );
  valCell.setAttribute ( "allowevents", "true" );
  var valField    = document.createElementNS ( XUL_NS, "textfield" );
  if ( name  ) valField.setAttribute ( "id", name );
  if (!valField) return null;
  if ( value ) valField.setAttribute ( "value", value );
  valField.setAttribute ( "flex", "1" );
  valField.setAttribute ( "class", "plain" );
  //XXX JS errors (can't tell where!) are preventing this from firing
  valField.setAttribute ( "onfocus", "SelectItemWithTextfield("+name+")");
  valCell.appendChild ( valField );
  return valCell;
}

function SelectItemWithTextfield(id)
{
dump("*** SelectItemWithTextfield\n");
  var textfield = document.getItemById(id);
  if (textfield)
  {
    var treerow = textfield.parentNode.parentNode;
    var tree = treeerow.parentNode.parentNode;
    if (tree)
    {
      gSelecting = true;
      tree.selectedItem = textfield.parentNode;
      gSelecting = false;
    }
  }
}

// When a "name" treecell is selected, shift focus to the textfield
function SelectTreeItem(tree)
{
  // Prevent infinite loop -- SetTextfieldFocusById triggers recursive call
  if (gSelecting) return;
  gSelecting = true;  
  if (tree && tree.selectedItems && tree.selectedItems.length)
  {
    // 2nd cell (value column) contains the textfield
    var textfieldCell = tree.selectedItems[0].firstChild.firstChild.nextSibling;
    if (textfieldCell)
    {
      // Select its contents and set focus
      SetTextfieldFocusById(textfieldCell.firstChild.id);
    }
  }
  gSelecting = false;
}

// todo: implement attribute parsing, e.g. colorpicker appending, etc.
function AttributeParser( name, value )
{
  
}

