/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Selection List Properties Dialog.
 *
 * The Initial Developer of the Original Code is
 * Neil Rashbrook.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Neil Rashbrook <neil@parkwaycc.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Global variables

var hasValue;
var oldValue;
var insertNew;
var globalArray;
var selectElement;
var currentObject = null;
var selectedOption = 0;
var selectedOptionCount = 0;

// Utility functions

function GetObjectForTreeItem(treeItem)
{
  return globalArray[parseInt(treeItem.getAttribute("index"))];
}

function InsertBefore(parent, before, after)
{
  var savedObject = currentObject;
  before.parentObject = parent;
  parent.element.insertBefore(before.element, after.element);
  parent.treeChildren.insertBefore(before.treeItem, after.treeItem);
  gDialog.tree.focus();
  selectTreeItem(savedObject.treeItem);
  gDialog.previousButton.disabled = !savedObject.canMoveUp();
  gDialog.nextButton.disabled = !savedObject.canMoveDown();
}

function AppendChild(parent, option)
{
  var savedObject = currentObject;
  option.parentObject = parent;
  parent.treeChildren.appendChild(option.treeItem);
  parent.element.appendChild(option.element);
  gDialog.tree.focus();
  selectTreeItem(savedObject.treeItem);
  gDialog.previousButton.disabled = !savedObject.canMoveUp();
  gDialog.nextButton.disabled = !savedObject.canMoveDown();
}

function UpdateSelectMultiple()
{
  if (selectedOptionCount > 1)
  {
    gDialog.selectMultiple.checked = true;
    gDialog.selectMultiple.disabled = true;
  }
  else
    gDialog.selectMultiple.disabled = false;
}

function createXUL(localName)
{
  return document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", localName);
}

// OPTION element wrapper object

function optionObject(parent, option)
{
  if (option.hasAttribute("selected"))
    selectedOptionCount++;
  this.globalIndex = globalArray.length;
  globalArray[globalArray.length++] = this;
  this.parentObject = parent;
  this.element = option;
  this.treeItem = createXUL("treeitem");
  this.treeRow = createXUL("treerow");
  this.treeCellText = createXUL("treecell");
  this.treeCellValue = createXUL("treecell");
  this.treeCellSelected = createXUL("treecell");
  this.treeItem.setAttribute("index", this.globalIndex);
  this.treeItem.setAttribute("container", "true");
  this.treeItem.setAttribute("empty", "true");
  this.treeCellText.setAttribute("class", "treecell-indent");
  this.treeCellText.setAttribute("notwisty", "true");
  this.treeCellText.setAttribute("label", option.text);
  if (option.hasAttribute("value"))
    this.treeCellValue.setAttribute("label", option.value);
  else
    this.treeCellValue.setAttribute("label", option.text);
  this.treeCellSelected.setAttribute("properties", "checked-"+option.hasAttribute("selected"));
  this.treeRow.appendChild(this.treeCellText);
  this.treeRow.appendChild(this.treeCellValue);
  this.treeRow.appendChild(this.treeCellSelected);
  this.treeItem.appendChild(this.treeRow);
  parent.treeChildren.appendChild(this.treeItem);
  gDialog.treeItem.setAttribute("open", "true");
}

optionObject.prototype.onSpace = function onSpace()
{
  if (this.element.hasAttribute("selected"))
  {
    selectedOptionCount--;
    this.element.removeAttribute("selected");
    this.treeCellSelected.setAttribute("properties", "checked-false");
    gDialog.optionSelected.setAttribute("checked", "false");
    selectedOption = 0;
  }
  else
  {
    if (gDialog.selectMultiple.checked || !globalArray[selectedOption].element.hasAttribute("selected"))
      selectedOptionCount++;
    else
    {
      globalArray[selectedOption].element.removeAttribute("selected");
      globalArray[selectedOption].treeCellSelected.setAttribute("properties", "checked-false");
    }
    this.element.setAttribute("selected", "");
    this.treeCellSelected.setAttribute("properties", "checked-true");
    gDialog.optionSelected.setAttribute("checked", "true");
    selectedOption = this.globalIndex;
  }
  UpdateSelectMultiple();
};

optionObject.prototype.onFocus = function onFocus()
{
  gDialog.optionText.value = this.element.text;
  hasValue = this.element.hasAttribute("value");
  oldValue = this.element.value;
  gDialog.optionHasValue.checked = hasValue;
  gDialog.optionValue.value = hasValue ? this.element.value : this.element.text;
  gDialog.optionSelected.checked = this.element.hasAttribute("selected");
  gDialog.optionDisabled.checked = this.element.hasAttribute("disabled");
  gDialog.selectDeck.setAttribute("selectedIndex", "2");
};

optionObject.prototype.onBlur = function onBlur()
{
  this.element.text = gDialog.optionText.value;
  if (gDialog.optionHasValue.checked)
    this.element.value = gDialog.optionValue.value;
  else
    this.element.removeAttribute("value");
  if (gDialog.optionSelected.checked)
    this.element.setAttribute("selected", "");
  else
    this.element.removeAttribute("selected");
  if (gDialog.optionDisabled.checked)
    this.element.setAttribute("disabled", "");
  else
    this.element.removeAttribute("disabled");
};

optionObject.prototype.canDestroy = function canDestroy(prompt)
{
  return true;
/*return !prompt ||
    ConfirmWithTitle(GetString("DeleteOption"),
                     GetString("DeleteOptionMsg"),
                     GetString("DeleteOption"));*/
};

optionObject.prototype.destroy = function destroy()
{
  if (this.element.hasAttribute("selected"))
  {
    selectedOptionCount--;
    UpdateSelectMultiple();
  }
  this.parentObject.removeChild(this);
};

optionObject.prototype.canMoveUp = function canMoveUp()
{
  return this.treeItem.previousSibling || this.parentObject != gDialog;
}

optionObject.prototype.moveUp = function moveUp()
{
  if (this.treeItem.previousSibling)
    GetObjectForTreeItem(this.treeItem.previousSibling).insertAfter(this);
  else
    InsertBefore(gDialog, this, this.parentObject);
}

optionObject.prototype.insertAfter = function insertAfter(option)
{
  InsertBefore(this.parentObject, option, this);
}

optionObject.prototype.canMoveDown = function canMoveDown()
{
  return this.treeItem.nextSibling || this.parentObject != gDialog;
}

optionObject.prototype.moveDown = function moveDown()
{
  if (this.treeItem.nextSibling)
    GetObjectForTreeItem(this.treeItem.nextSibling).insertBefore(this);
  else if (this.parentObject.treeItem.nextSibling)
    InsertBefore(gDialog, this, GetObjectForTreeItem(this.parentObject.treeItem.nextSibling));
  else
    AppendChild(gDialog, this);
}

optionObject.prototype.insertBefore = function insertBefore(option)
{
  InsertBefore(this.parentObject, this, option);
}

optionObject.prototype.appendChild = function appendChild(child)
{
  return this.parentObject.appendChild(child);
};

// OPTGROUP element wrapper object

function optgroupObject(parent, optgroup)
{
  this.globalIndex = globalArray.length;
  globalArray[globalArray.length++] = this;
  this.parentObject = parent;
  this.element = optgroup;
  this.treeItem = createXUL("treeitem");
  this.treeRow = createXUL("treerow");
  this.treeCell = createXUL("treecell");
  this.treeChildren = createXUL("treechildren");
  this.treeItem.setAttribute("index", this.globalIndex);
  this.treeItem.setAttribute("container", "true");
  this.treeItem.setAttribute("empty", "true");
  this.treeItem.setAttribute("open", "true");
  this.treeCell.setAttribute("class", "treecell-indent");
  this.treeCell.setAttribute("notwisty", "true");
  this.treeCell.setAttribute("label", optgroup.label);
  this.treeRow.appendChild(this.treeCell);
  this.treeItem.appendChild(this.treeRow);
  this.treeItem.appendChild(this.treeChildren);
  parent.treeChildren.appendChild(this.treeItem);
  gDialog.treeItem.setAttribute("open", "true");
  for (var child = optgroup.firstChild; child; child = child.nextSibling)
    if (child.tagName == "OPTION")
      new optionObject(this, child);
}

optgroupObject.prototype.onSpace = function onSpace()
{
};

optgroupObject.prototype.onFocus = function onFocus()
{
  gDialog.optgroupLabel.value = this.element.label;
  gDialog.optgroupDisabled.checked = this.element.disabled;
  gDialog.selectDeck.setAttribute("selectedIndex", "1");
};

optgroupObject.prototype.onBlur = function onBlur()
{
  this.element.label = gDialog.optgroupLabel.value;
  this.element.disabled = gDialog.optgroupDisabled.checked;
};

optgroupObject.prototype.canDestroy = function canDestroy(prompt)
{
  return !this.element.firstChild;
/*return !this.element.firstChild && (!prompt ||
    ConfirmWithTitle(GetString("DeleteOptGroup"),
                     GetString("DeleteOptGroupMsg"),
                     GetString("DeleteOptGroup")));
*/
};

optgroupObject.prototype.destroy = function destroy()
{
  gDialog.removeChild(this);
};

optgroupObject.prototype.canMoveUp = function canMoveUp()
{
  return this.treeItem.previousSibling;
}

optgroupObject.prototype.moveUp = function moveUp()
{
  InsertBefore(this.parentObject, this, GetObjectForTreeItem(this.treeItem.previousSibling));
}

optgroupObject.prototype.insertBefore = function insertBefore(option)
{
  if (this.treeChildren.firstChild)
    InsertBefore(this, option, GetObjectForTreeItem(this.treeChildren.firstChild));
  else
    AppendChild(this, option);
}

optgroupObject.prototype.canMoveDown = function canMoveDown()
{
  return this.treeItem.nextSibling;
}

optgroupObject.prototype.moveDown = function moveDown()
{
  InsertBefore(this.parentObject, GetObjectForTreeItem(this.treeItem.nextSibling), this);
}

optgroupObject.prototype.insertAfter = function insertAfter(option)
{
  AppendChild(this, option);
}

optgroupObject.prototype.appendChild = function appendChild(child)
{
  this.element.appendChild(child);
  return new optionObject(this, child);
};

optgroupObject.prototype.removeChild = function removeChild(child)
{
  this.element.removeChild(child.element);
  this.treeChildren.removeChild(child.treeItem);
  globalArray[child.globalIndex] = null;
};

// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;

  // Get a single selected select element
  var tagName = "select";
  selectElement = editorShell.GetSelectedElement(tagName);

  if (selectElement)
    // We found an element and don't need to insert one
    insertNew = false;
  else
  {
    insertNew = true;

    // We don't have an element selected,
    //  so create one with default attributes

    selectElement = editorShell.CreateElementWithDefaults(tagName);
    if(!selectElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
  }

  // SELECT element wrapper object
  gDialog = {
    accept:           document.documentElement.getButton("accept"),
    selectDeck:       document.getElementById("SelectDeck"),
    selectName:       document.getElementById("SelectName"),
    selectSize:       document.getElementById("SelectSize"),
    selectMultiple:   document.getElementById("SelectMultiple"),
    selectDisabled:   document.getElementById("SelectDisabled"),
    selectTabIndex:   document.getElementById("SelectTabIndex"),
    optgroupLabel:    document.getElementById("OptGroupLabel"),
    optgroupDisabled: document.getElementById("OptGroupDisabled"),
    optionText:       document.getElementById("OptionText"),
    optionHasValue:   document.getElementById("OptionHasValue"),
    optionValue:      document.getElementById("OptionValue"),
    optionSelected:   document.getElementById("OptionSelected"),
    optionDisabled:   document.getElementById("OptionDisabled"),
    removeButton:     document.getElementById("RemoveButton"),
    previousButton:   document.getElementById("PreviousButton"),
    nextButton:       document.getElementById("NextButton"),
    tree:             document.getElementById("SelectTree"),
    treeCols:         document.getElementById("SelectCols"),
    treeItem:         document.getElementById("SelectItem"),
    treeCell:         document.getElementById("SelectCell"),
    treeChildren:     document.getElementById("SelectChildren"),
    element:          selectElement.cloneNode(false),
    globalIndex:      0,
    onSpace:          function onSpace() {},
    onFocus:          function onFocus()
    {
      gDialog.selectName.value = this.element.getAttribute("name");
      gDialog.selectSize.value = this.element.getAttribute("size");
      gDialog.selectMultiple.checked = this.element.hasAttribute("multiple");
      gDialog.selectDisabled.checked = this.element.hasAttribute("disabled");
      gDialog.selectTabIndex.value = this.element.getAttribute("tabindex");
      this.selectDeck.setAttribute("selectedIndex", "0");
      onNameInput();
    },
    onBlur:           function onBlur()
    {
      this.element.setAttribute("name", gDialog.selectName.value);
      if (gDialog.selectSize.value)
        this.element.setAttribute("size", gDialog.selectSize.value);
      else
        this.element.removeAttribute("size");
      if (gDialog.selectMultiple.checked)
        this.element.setAttribute("multiple", "");
      else
        this.element.removeAttribute("multiple");
      if (gDialog.selectDisabled.checked)
        this.element.setAttribute("disabled", "");
      else
        this.element.removeAttribute("disabled");
      if (gDialog.selectTabIndex.value)
        this.element.setAttribute("tabindex", gDialog.selectTabIndex.value);
      else
        this.element.removeAttribute("tabindex");
    },
    appendChild:      function appendChild(child)
    {
      if (child.tagName == "OPTION")
      {
        this.element.appendChild(child)
        return new optionObject(this, child);
      }
      if (child.tagName == "OPTGROUP")
      {
        this.element.appendChild(child)
        return new optgroupObject(this, child);
      }
      return null;
    },
    removeChild:      function removeChild(child)
    {
      this.treeChildren.removeChild(child.treeItem);
      globalArray[child.globalIndex] = null;
    },
    canDestroy:       function canDestroy(prompt)
    {
      return false;
    },
    canMoveUp:        function canMoveUp()
    {
      return false;
    },
    canMoveDown:      function canMoveDown()
    {
      return false;
    }
  }
  globalArray = [gDialog];

  // Workaround for tree scrollbar bug
  for (var col = gDialog.treeCols.firstChild; col.nextSibling; col = col.nextSibling)
  {
    col.setAttribute("width", col.boxObject.width);
    col.removeAttribute("flex");
  }

  // We modify the actual option and optgroup elements so clone them first
  for (var child = selectElement.firstChild; child; child = child.nextSibling)
    gDialog.appendChild(child.cloneNode(true));

  UpdateSelectMultiple();

  selectTreeItem(gDialog.treeItem);
  onNameInput();

  SetTextboxFocus(gDialog.selectName);

  SetWindowLocation();
}

function InitDialog()
{
  currentObject.onFocus();
}

function ValidateData()
{
  currentObject.onBlur();
  return true;
}

function onAccept()
{
  // All values are valid - copy to actual element in doc or
  //   element created to insert
  ValidateData();

  try {
    // Coalesce into one undo transaction
    editorShell.BeginBatchChanges();

    editorShell.CloneAttributes(selectElement, gDialog.element);

    if (insertNew)
      // 'true' means delete the selection before inserting
      editorShell.InsertElementAtSelection(selectElement, true);

    while (selectElement.firstChild)
      editorShell.DeleteElement(selectElement.firstChild);

    var newNodes = gDialog.element.childNodes;
    for (var offset = 0; offset < newNodes.length; offset++)
      editorShell.InsertElement(newNodes[offset], selectElement, offset, true);
  } finally {
    editorShell.EndBatchChanges();
  }

  SaveWindowLocation();

  return true;
}

// Button actions
function AddOption()
{
  var optionElement = editorShell.CreateElementWithDefaults("option");
  var optionObject = currentObject.appendChild(optionElement);
  selectTreeItem(optionObject.treeItem);
  SetTextboxFocus(gDialog.optionText);
}

function AddOptGroup()
{
  var optgroupElement = editorShell.CreateElementWithDefaults("optgroup");
  var optgroupObject = gDialog.appendChild(optgroupElement);
  selectTreeItem(optgroupObject.treeItem);
  SetTextboxFocus(gDialog.optgroupLabel);
}

function RemoveElement()
{
  if (currentObject.canDestroy(true))
  {
    var selection = currentObject.treeItem;
    selection = selection.nextSibling || selection.previousSibling || selection.parentNode.parentNode;
    currentObject.destroy();
    selectTreeItem(selection);
    gDialog.tree.focus();
  }
}

// Event handlers
function onTreeSelect(event)
{
  if (currentObject)
    currentObject.onBlur();
  var selectedItem = gDialog.tree.contentView.getItemAtIndex(gDialog.tree.currentIndex);
  if (selectedItem)
  {
    currentObject = globalArray[parseInt(selectedItem.getAttribute("index"))];
    currentObject.onFocus();
  }
  else
    currentObject = gDialog;
  gDialog.removeButton.disabled = !currentObject.canDestroy(false);
  gDialog.previousButton.disabled = !currentObject.canMoveUp();
  gDialog.nextButton.disabled = !currentObject.canMoveDown();
  globalElement = currentObject.element;
}

function onTreeKeyUp(event)
{
  if (event.keyCode == event.DOM_VK_SPACE)
    currentObject.onSpace();
}

function onTreeClicked(event)
{
  var row = {}, col = {}, obj = {};
  gDialog.tree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, obj);
  
  if (col.value == "SelectSelCol") {
    var selection = gDialog.tree.contentView.getItemAtIndex(row.value);
    currentObject = globalArray[parseInt(selection.getAttribute("index"))];
    if ("treeCellSelected" in currentObject) {
      selectTreeItem(selection);
      currentObject.onSpace();
    }
  }
  gDialog.tree.focus();
}

var timeout = 0;

function copyValue(textbox, treecell, delay)
{
  if (timeout) clearTimeout(timeout);
  if (delay)
    timeout = setTimeout(copyValue, 800, textbox, treecell, false);
  else {
    timeout = 0;
    treecell.setAttribute("label", textbox.value);
  }
}

function onNameInput()
{
  var disabled = !gDialog.selectName.value;
  if (gDialog.accept.disabled != disabled)
    gDialog.accept.disabled = disabled;
  copyValue(gDialog.selectName, gDialog.treeCell, true);
}

function onNameChange()
{
  copyValue(gDialog.selectName, gDialog.treeCell, false);
}

function onLabelInput()
{
  copyValue(gDialog.optgroupLabel, currentObject.treeCell, true);
}

function onLabelChange()
{
  copyValue(gDialog.optgroupLabel, currentObject.treeCell, false);
}

function copyText(delay)
{
  if (timeout) clearTimeout(timeout);
  if (delay)
    timeout = setTimeout(copyText, 800, false);
  else {
    timeout = 0;
    currentObject.treeCellText.setAttribute("label", gDialog.optionText.value);
    if (!hasValue)
    {
      gDialog.optionValue.value = gDialog.optionText.value;
      currentObject.treeCellValue.setAttribute("label", gDialog.optionText.value);
    }
  }
}

function onTextInput()
{
  copyText(true);
}

function onTextChange()
{
  copyText(false);
}

function onValueInput()
{
  gDialog.optionHasValue.checked = hasValue = true;
  oldValue = gDialog.optionValue.value;
  copyValue(gDialog.optionValue, currentObject.treeCellValue, true);
}

function onValueChange()
{
  copyValue(gDialog.optionValue, currentObject.treeCellValue, false);
}

function onHasValueClick()
{
  hasValue = gDialog.optionHasValue.checked;
  if (hasValue)
  {
    gDialog.optionValue.value = oldValue;
  }
  else
  {
    oldValue = gDialog.optionValue.value;
    gDialog.optionValue.value = gDialog.optionText.value;
  }
  currentObject.treeCellValue.setAttribute("label", gDialog.optionValue.value);
}

function onSelectMultipleClick()
{
  if (!gDialog.selectMultiple.checked && selectedOptionCount == 1 && !selectedOption)
    while (!globalArray[selectedOption] || !globalArray[selectedOption].element.hasAttribute("selected"))
      selectedOption++;
}

function selectTreeItem(aItem)
{
  var itemIndex = gDialog.tree.contentView.getIndexOfItem(aItem);
  gDialog.tree.treeBoxObject.selection.select(itemIndex);
  gDialog.tree.treeBoxObject.ensureRowIsVisible(itemIndex);
}