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

function BuildHTMLAttributeNameList()
{
  ClearMenulist(gDialog.AddHTMLAttributeNameInput);
  
  var elementName = gElement.localName.toLowerCase();
  var attNames = gHTMLAttr[elementName];

  if (attNames && attNames.length)
  {
    var menuitem;

    for (var i = 0; i < attNames.length; i++)
    {
      var name = attNames[i];
      var limitFirstChar;

      if (name == "_core")
      {
        // Signal to append the common 'core' attributes.
        for (var j = 0; j < gCoreHTMLAttr.length; j++)
        {
          name = gCoreHTMLAttr[j];

          // "limitFirstChar" is the only filtering rule used for core attributes as of 8-20-01
          // Add more rules if necessary          
          limitFirstChar = name.indexOf("^") >= 0;
          if (limitFirstChar)
          {
            menuitem = AppendStringToMenulist(gDialog.AddHTMLAttributeNameInput, name.replace(/\^/g, ""));
            menuitem.setAttribute("limitFirstChar", "true");
          }
          else
            AppendStringToMenulist(gDialog.AddHTMLAttributeNameInput, name);
        }
      }
      else if (name == "-")
      {
        // Signal for separator
        var popup = gDialog.AddHTMLAttributeNameInput.firstChild;
        if (popup)
        {
          var sep = document.createElementNS(XUL_NS, "menuseparator");
          if (sep)
            popup.appendChild(sep);
        }        
      }
      else
      {
        // Get information about value filtering
        var forceOneChar = name.indexOf("!") >= 0;
        var forceInteger = name.indexOf("#") >= 0;
        var forceSignedInteger = name.indexOf("+") >= 0;
        var forceIntOrPercent = name.indexOf("%") >= 0;
        limitFirstChar = name.indexOf("\^") >= 0;
        //var required = name.indexOf("$") >= 0;

        // Strip flag characters
        name = name.replace(/[!^#%$+]/g, "");

        menuitem = AppendStringToMenulist(gDialog.AddHTMLAttributeNameInput, name);
        if (menuitem)
        {
          // Signify "required" attributes by special style
          //TODO: Don't do this until next version, when we add
          //      explanatory text and an 'Autofill Required Attributes' button
          //if (required)
          //  menuitem.setAttribute("class", "menuitem-highlight-1");

          // Set flags to filter value input
          if (forceOneChar)
            menuitem.setAttribute("forceOneChar","true");
          if (limitFirstChar)
            menuitem.setAttribute("limitFirstChar", "true");
          if (forceInteger)
            menuitem.setAttribute("forceInteger", "true");
          if (forceSignedInteger)
            menuitem.setAttribute("forceSignedInteger", "true");
          if (forceIntOrPercent)
            menuitem.setAttribute("forceIntOrPercent", "true");
        }
      }
    }
  }

  // Always start with empty input fields
  ClearHTMLInputWidgets();
}

// build attribute list in tree form from element attributes
function BuildHTMLAttributeTable()
{
  var nodeMap = gElement.attributes;
  var i;
  if (nodeMap.length > 0) 
  {
    var added = false;
    for(i = 0; i < nodeMap.length; i++)
    {
      if ( CheckAttributeNameSimilarity( nodeMap[i].nodeName, HTMLAttrs ) ||
          IsEventHandler( nodeMap[i].nodeName ) ||
          TrimString( nodeMap[i].nodeName.toLowerCase() ) == "style" ) {
        continue;   // repeated or non-HTML attribute, ignore this one and go to next
      }
      var name  = nodeMap[i].nodeName.toLowerCase();
      var value = gElement.getAttribute ( nodeMap[i].nodeName );
      if ( name.indexOf("_moz") != 0 &&
           AddTreeItem(name, value, "HTMLAList", HTMLAttrs) )
        added = true;
    }

    if (added)
      SelectHTMLTree(0);
  }
}

// add or select an attribute in the tree widget
function onChangeHTMLAttribute()
{
  var name = TrimString(gDialog.AddHTMLAttributeNameInput.value);
  if (!name)
    return;

  var value = TrimString(gDialog.AddHTMLAttributeValueInput.value);

  // First try to update existing attribute
  // If not found, add new attribute
  if (!UpdateExistingAttribute( name, value, "HTMLAList" ) && value)
    AddTreeItem (name, value, "HTMLAList", HTMLAttrs);
}

function ClearHTMLInputWidgets()
{
  gDialog.AddHTMLAttributeTree.clearItemSelection();
  gDialog.AddHTMLAttributeNameInput.value ="";
  gDialog.AddHTMLAttributeValueInput.value = "";
  gDialog.AddHTMLAttributeNameInput.inputField.focus();
}

function onSelectHTMLTreeItem()
{
  if (!gDoOnSelectTree)
    return;

  var tree = gDialog.AddHTMLAttributeTree;
  if (tree && tree.selectedItems && tree.selectedItems.length)
  {
    var inputName = TrimString(gDialog.AddHTMLAttributeNameInput.value).toLowerCase();
    var selectedName = tree.selectedItems[0].firstChild.firstChild.getAttribute("label");

    if (inputName == selectedName)
    {
      // Already editing selected name - just update the value input
      gDialog.AddHTMLAttributeValueInput.value = GetTreeItemValueStr(tree.selectedItems[0]);
    }
    else
    {
      gDialog.AddHTMLAttributeNameInput.value = selectedName;

      // Change value input based on new selected name
      onInputHTMLAttributeName();
    }
  }
}

function onInputHTMLAttributeName()
{
  var attName = TrimString(gDialog.AddHTMLAttributeNameInput.value).toLowerCase();

  // Clear value widget, but prevent triggering update in tree
  gUpdateTreeValue = false;
  gDialog.AddHTMLAttributeValueInput.value = "";
  gUpdateTreeValue = true; 

  if (attName)
  {
    // Get value list for current attribute name
    var valueListName;

    // Most elements have the "dir" attribute,
    //   so we have just one array for the allowed values instead
    //   requiring duplicate entries for each element in EdAEAttributes.js
    if (attName == "dir")
      valueListName = "all_dir";
    else
      valueListName = gElement.localName.toLowerCase() + "_" + attName;

    // Strip off leading "_" we sometimes use (when element name is reserved word)
    if (valueListName[0] == "_")
      valueListName = valueListName.slice(1);

    var newValue = "";
    var listLen = 0;

    // Index to which widget we were using to edit the value
    var deckIndex = gDialog.AddHTMLAttributeValueDeck.getAttribute("selectedIndex");

    if (valueListName in gHTMLAttr)
    {
      var valueList = gHTMLAttr[valueListName];

      listLen = valueList.length;
      if (listLen > 0)
        newValue = valueList[0];

      // Note: For case where "value list" is actually just 
      // one (default) item, don't use menulist for that
      if (listLen > 1)
      {
        ClearMenulist(gDialog.AddHTMLAttributeValueMenulist);

        if (deckIndex != "1")
        {
          // Switch to using editable menulist
          gDialog.AddHTMLAttributeValueInput = gDialog.AddHTMLAttributeValueMenulist;
          gDialog.AddHTMLAttributeValueDeck.setAttribute("selectedIndex", "1");
        }
        // Rebuild the list
        for (var i = 0; i < listLen; i++)
        {
          if (valueList[i] == "-")
          {
            // Signal for separator
            var popup = gDialog.AddHTMLAttributeValueInput.firstChild;
            if (popup)
            {
              var sep = document.createElementNS(XUL_NS, "menuseparator");
              if (sep)
                popup.appendChild(sep);
            }        
          } else {
            AppendStringToMenulist(gDialog.AddHTMLAttributeValueMenulist, valueList[i]);
          }
        }
      }
    }
    
    if (listLen <= 1 && deckIndex != "0")
    {
      // No list: Use textbox for input instead
      gDialog.AddHTMLAttributeValueInput = gDialog.AddHTMLAttributeValueTextbox;
      gDialog.AddHTMLAttributeValueDeck.setAttribute("selectedIndex", "0");
    }

    // If attribute already exists in tree, use associated value,
    //  else use default found above
    var existingValue = GetAndSelectExistingAttributeValue(attName, "HTMLAList");
    if (existingValue)
      newValue = existingValue;
      
    gDialog.AddHTMLAttributeValueInput.value = newValue;
  }
}

function onInputHTMLAttributeValue()
{
  if (!gUpdateTreeValue)
    return;

  var name = TrimString(gDialog.AddHTMLAttributeNameInput.value);
  if (!name)
    return;

  // Trim spaces only from left since we must allow spaces within the string
  //  (we always reset the input field's value below)
  var value = TrimStringLeft(gDialog.AddHTMLAttributeValueInput.value);
  if (value)
  {
    // Do value filtering based on type of attribute
    // (Do not use "LimitStringLength()" and "forceInteger()"
    //  to avoid multiple reseting of input's value and flickering)
    var selectedItem = gDialog.AddHTMLAttributeNameInput.selectedItem;

    if (selectedItem)
    {
      if ( selectedItem.getAttribute("forceOneChar") == "true" &&
           value.length > 1 )
        value = value.slice(0, 1);

      if ( selectedItem.getAttribute("forceIntOrPercent") == "true" )
      {
        // Allow integer with optional "%" as last character
        var percent = TrimStringRight(value).slice(-1);
        value = value.replace(/\D+/g,"");
        if (percent == "%")
          value += percent;
      }
      else if ( selectedItem.getAttribute("forceInteger") == "true" )
      {
        value = value.replace(/\D+/g,"");
      }
      else if ( selectedItem.getAttribute("forceSignedInteger") == "true" )
      {
        // Allow integer with optional "+" or "-" as first character
        var sign = value[0];
        value = value.replace(/\D+/g,"");
        if (sign == "+" || sign == "-")
          value = sign + value;
      }
      
      // Special case attributes 
      if (selectedItem.getAttribute("limitFirstChar") == "true")
      {
        // Limit first character to letter, and all others to 
        //  letters, numbers, and a few others
        value = value.replace(/^[^a-zA-Z\u0080-\uFFFF]/, "").replace(/[^a-zA-Z0-9_\.\-\:\u0080-\uFFFF]+/g,'');
      }

      // Update once only if it changed
      if (value != gDialog.AddHTMLAttributeValueInput.value)
        gDialog.AddHTMLAttributeValueInput.value = value;
    }
  }

  // Update value in the tree list
  // If not found, add new attribute
  if ( !UpdateExistingAttribute(name, value, "HTMLAList" ) && value)
    AddTreeItem(name, value, "HTMLAList", HTMLAttrs);
}

function editHTMLAttributeValue(targetCell)
{
  if (IsNotTreeHeader(targetCell))
    gDialog.AddHTMLAttributeValueInput.inputField.select();
}


// update the object with added and removed attributes
function UpdateHTMLAttributes()
{
  var HTMLAList = document.getElementById("HTMLAList");
  var i;

  // remove removed attributes
  for (i = 0; i < HTMLRAttrs.length; i++)
  {
    var name = HTMLRAttrs[i];

    // We can't use getAttribute to figure out if attribute already
    //  exists for attributes that don't require a value
    // This gets the attribute NODE from the attributes NamedNodeMap
    if (gElement.attributes.getNamedItem(name))
      doRemoveAttribute(name);
  }

  // Set added or changed attributes
  for( i = 0; i < HTMLAList.childNodes.length; i++)
  {
    var item = HTMLAList.childNodes[i];
    doSetAttribute( GetTreeItemAttributeStr(item), GetTreeItemValueStr(item));
  }
}

function RemoveHTMLAttribute()
{
  var treechildren = gDialog.AddHTMLAttributeTree.lastChild;

  // We only allow 1 selected item
  if (gDialog.AddHTMLAttributeTree.selectedItems.length)
  {
    var item = gDialog.AddHTMLAttributeTree.selectedItems[0];
    var attr = GetTreeItemAttributeStr(item);

    // remove the item from the attribute array
    HTMLRAttrs[HTMLRAttrs.length] = attr;
    RemoveNameFromAttArray(attr, HTMLAttrs);

    // Remove the item from the tree
    treechildren.removeChild(item);

    // Clear inputs and selected item in tree
    ClearHTMLInputWidgets();
  }
}

function SelectHTMLTree( index )
{

  gDoOnSelectTree = false;
  try {
    gDialog.AddHTMLAttributeTree.selectedIndex = index;
  } catch (e) {}
  gDoOnSelectTree = true;
}
