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

var HTMLRAttrs  = [];   // removed html attributes
var CSSRAttrs   = [];   // removed css attributes
var JSERAttrs   = [];   // removed js events

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

  // load string bundle
  bundle = srGetStrBundle("chrome://editor/locale/editor.properties");
    
  // Element to edit is passed in
  element = window.arguments[1];
  if (!element || element == "undefined") {
    dump("Advanced Edit: Element to edit not supplied\n");
    window.close();  
  }
  dump("*** Element passed into Advanced Edit: "+element+" ***\n");

  // place the tag name in the header
  var tagLabel = document.getElementById("tagLabel");
  tagLabel.setAttribute("value", ("<" + element.nodeName + ">"));

  // Create dialog object to store controls for easy access
  dialog                            = new Object;
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
  dump("in onOK\n")
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
// Note: now changing this to remove all selected ITEMS. this makes it easier. 
function doRemoveAttribute( which )
{
  if(which.nodeName != "tree") {
    var tree = which.parentNode;
    while ( tree.nodeName != "tree" )
    {
      tree = tree.parentNode; // climb up the tree one notch
    } // now we are pointing at the tree element
  } else
    tree = which;
  
  var kids = tree.lastChild;  // treechildren element of tree
  var selArray = [];
  for ( var i = 0; i < tree.selectedItems.length; i++ )
  {
    var item = tree.selectedItems[i];
    // add to array of removed items for the particular panel that is displayed
    var name = item.firstChild.firstChild;
    switch ( tree.id ) {
      case "HTMLATree":
        HTMLRAttrs[HTMLRAttrs.length] = TrimString(name.getAttribute("value"));
        dump("HTMLRAttrs[" + (HTMLRAttrs.length - 1) + "]: " + HTMLRAttrs[HTMLRAttrs.length-1] + "\n");
        break;
      case "CSSATree":   
        CSSRAttrs[CSSRAttrs.length] = TrimString(name.getAttribute("value"));
        break;
      case "JSEATree":
        JSERAttrs[JSERAttrs.length] = TrimString(name.getAttribute("value"));
        break;      
      default: break;
    }
    selArray[i] = item;
  }
  // need to do this in a separate loop because selectedItems is NOT STATIC and 
  // this causes problems.
  for ( var i = 0; i < selArray.length; i++ )
  {
    // remove the item
    kids.removeChild ( selArray[i] );
  } 
}

/**
 * function   : void doAddAttribute( DOMElement which );
 * parameters : DOMElement referring to element context-clicked
 * returns    : nothing
 * desc.      : focusses the add attribute "name" field in the current pane.
 **/
function doAddAttribute(which)
{
  if(which.nodeName != "tree") {
    var tree = which.parentNode;
    while ( tree.nodeName != "tree" )
    {
      tree = tree.parentNode; // climb up the tree one notch
    } // now we are pointing at the tree element
  } else
    tree = which;

  switch(tree.id) {
    case "HTMLATree":
      SetTextfieldFocus(document.getElementById("AddHTMLAttributeNameInput"));
      break;
    case "CSSATree":
      SetTextfieldFocus(document.getElementById("AddCSSAttributeNameInput"));
      break;
    case "JSEATree":
      SetTextfieldFocus(document.getElementById("AddJSEAttributeNameInput"));
      break;
    default:
      break;
  }
}

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

// adds a generalised treeitem.
function AddTreeItem ( name, value, treekids, attArray, valueCaseFunc )
{
  attArray[attArray.length] = name;
  var treekids    = document.getElementById ( treekids );
  var treeitem    = document.createElementNS ( XUL_NS, "treeitem" );
  var treerow     = document.createElementNS ( XUL_NS, "treerow" );
  var attrcell    = document.createElementNS ( XUL_NS, "treecell" );
  attrcell.setAttribute( "class", "propertylist" );
  attrcell.setAttribute( "value", name.toLowerCase() );
  treerow.appendChild ( attrcell );
  
  if ( !valueCaseFunc ) {
    // no handling function provided, create default cell.
    var valcell = CreateCellWithField ( name, value);
    treerow.appendChild ( valcell );
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
  var valcell     = document.createElementNS ( XUL_NS, "treecell" );
  valcell.setAttribute ( "class", "value propertylist" );
  valcell.setAttribute ( "allowevents", "true" );
  var valField    = document.createElementNS ( XUL_NS, "textfield" );
  if ( name  ) valField.setAttribute ( "id", name );
  if ( value ) valField.setAttribute ( "value", value );
  valField.setAttribute ( "flex", "1" );
  valField.setAttribute ( "class", "plain" );
  valcell.appendChild ( valField );
  return valcell;
}

// todo: implement attribute parsing, e.g. colorpicker appending, etc.
function AttributeParser( name, value )
{
  
}

