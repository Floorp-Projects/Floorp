/* 
  The contents of this file are subject to the Netscape Public
  License Version 1.1 (the "License"); you may not use this file
  except in compliance with the License. You may obtain a copy of
  the License at http://www.mozilla.org/NPL/
  
  Software distributed under the License is distributed on an "AS
  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
  implied. See the License for the specific language governing
  rights and limitations under the License.
  
  The Original Code is Mozilla Communicator client code, released
  March 31, 1998.
  
  The Initial Developer of the Original Code is David Hyatt. 
  Portions created by David Hyatt are
  Copyright (C) 2002 David Hyatt. All
  Rights Reserved.
  
  Contributor(s): 
    David Hyatt (hyatt@apple.com)
    Blake Ross (blaker@netscape.com)

*/

var gToolbarChanged = false;
var gCurrentDragOverItem = null;
var gDraggingFromPalette = false;

function addItemToToolbar(newItem, newToolbar)
{
  cleanUpItemForAdding(newItem);
  var enclosure = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                       "toolbarpaletteitem");
  if (newItem.getAttribute("flex"))
    enclosure.setAttribute("flex", newItem.getAttribute("flex"));

  // Set a draggesture handler to allow drag-rearrange within the clone toolbar.
  enclosure.setAttribute("ondraggesture", "gDraggingFromPalette = false; nsDragAndDrop.startDrag(event, dragObserver)");
  enclosure.appendChild(newItem);
  newToolbar.appendChild(enclosure);
}

function buildDialog()
{
  var toolbar = window.opener.document.getElementById("nav-bar");
  var cloneToolbarBox = document.getElementById("cloned-bar-container");
  var paletteBox = document.getElementById("palette-box");
  var currentSet = toolbar.getAttribute("currentset");
  if (!currentSet)
    currentSet = toolbar.getAttribute("defaultset");
  currentSet = currentSet.split(",");

  // Create a new toolbar that will model the one the user is trying to customize.
  // We won't just cloneNode() because we want to wrap each element on the toolbar in a
  // <toolbarpaletteitem/>, to prevent them from getting events (so they aren't styled on
  // hover, etc.) and to allow us to style them in the new nsITheme world.
  var newToolbar = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "toolbar");
  newToolbar.id = "cloneToolbar";

  // Walk through and manually clone the children of the to-be-customized toolbar.
  // Make sure all buttons look enabled (and that textboxes are disabled).
  var toolbarItem = toolbar.firstChild;
  while (toolbarItem) {
    var newItem = toolbarItem.cloneNode(true);
    addItemToToolbar(newItem, newToolbar);
    toolbarItem = toolbarItem.nextSibling;
  }

  cloneToolbarBox.appendChild(newToolbar);
  
  // Now build up a palette of items.
  var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "hbox");
  currentRow.setAttribute("class", "paletteRow");

  var rowSlot = 0;
  var rowMax = 4;

  var node = toolbar.palette.firstChild;
  var isOnToolbar = false;
  while (node) {
    for (var i = 0; i < currentSet.length; ++i) {
      if (currentSet[i] == node.id) {
        isOnToolbar = true;
        break;
      }
    }
    if (isOnToolbar) {
      node = node.nextSibling;
      isOnToolbar = false;
      continue;
    }
    var paletteItem = node.cloneNode(true);
    cleanUpItemForAdding(paletteItem);

    if (rowSlot == rowMax) {
      // Append the old row.
      paletteBox.appendChild(currentRow);

      // Make a new row.
      currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "hbox");
      currentRow.setAttribute("class", "paletteRow");
      rowSlot = 0;
    } 

    rowSlot++;
    // Create an enclosure for the item.
    var enclosure = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                         "toolbarpaletteitem");
    enclosure.setAttribute("align", "center");
    enclosure.setAttribute("pack", "center");
    enclosure.setAttribute("flex", "1");
    enclosure.setAttribute("width", "0");
    enclosure.setAttribute("minheight", "0");
    enclosure.setAttribute("minwidth", "0");
    enclosure.setAttribute("ondraggesture", "gDraggingFromPalette = true; nsDragAndDrop.startDrag(event, dragObserver)");
 
    enclosure.appendChild(paletteItem);
    currentRow.appendChild(enclosure);

    node = node.nextSibling;
  }

  if (currentRow) { 
    // Remaining flex
    var remainingFlex = rowMax - rowSlot;
    if (remainingFlex > 0) {
      var spring = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "spacer");
      spring.setAttribute("flex", remainingFlex);
      currentRow.appendChild(spring);
    }

    paletteBox.appendChild(currentRow);
  }
  
  // Set a min height on the new toolbar so it doesn't shrink if all the buttons are removed.
  newToolbar.setAttribute("minheight", newToolbar.boxObject.height);
}

var dragObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction) {
    aXferData.data = new TransferDataSet();
    var data = new TransferData();
    data.addDataForFlavour("text/toolbaritem-id", aEvent.target.firstChild.id);
    aXferData.data.push(data);
  }
}

var toolbarDNDObserver = {
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    if (gCurrentDragOverItem)
      gCurrentDragOverItem.removeAttribute("dragactive");

    var dropTargetWidth = aEvent.target.boxObject.width;
    var dropTargetX = aEvent.target.boxObject.x;

    if (aEvent.clientX > (dropTargetX + (dropTargetWidth / 2)))
      gCurrentDragOverItem = aEvent.target.nextSibling;
    else
      gCurrentDragOverItem = aEvent.target;

    gCurrentDragOverItem.setAttribute("dragactive", "true");
    aDragSession.canDrop = true;
  },
  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var newButtonId = aXferData.data;
    var toolbar = document.getElementById("cloneToolbar");
    var item = null;
    if (gDraggingFromPalette) {
      var palette = document.getElementById("palette-box");
      var paletteItems = palette.getElementsByTagName("toolbarpaletteitem");
      var paletteItem;
      for (var i = 0; i < paletteItems.length; ++i) {
        paletteItem = paletteItems[i];
        if (paletteItem.firstChild.id == newButtonId) {
          item = paletteItem;
          break;
        }
      }

    }
    else {
      // If drag-rearranging within the toolbar, we want to move it to
      // the new location, so remove it here and we'll add it in the correct spot further down.
      var toolbarItem = toolbar.firstChild;
      while (toolbarItem) {
        if (toolbarItem.firstChild.id == newButtonId) {
          item = toolbarItem;
          break;
        }
        toolbarItem = toolbarItem.nextSibling;
      }
    }
       
    if (!item)
      return;

    // We have to remove the funky flex and width attributes that were set on
    // the item to space it properly in the palette.
    item.removeAttribute("flex");
    item.removeAttribute("width");
    
    // Set whatever flex the item in our enclosure has on the enclosure,
    // so that we flex properly.
    if (item.firstChild.getAttribute("flex"))
      item.setAttribute("flex", item.firstChild.getAttribute("flex"));
    item.setAttribute("ondraggesture", "gDraggingFromPalette = false; nsDragAndDrop.startDrag(event, dragObserver);");
 
    if (gCurrentDragOverItem.id == "cloneToolbar")
      toolbar.appendChild(item);
    else
      toolbar.insertBefore(item, gCurrentDragOverItem);
    gCurrentDragOverItem.removeAttribute("dragactive");
    gCurrentDragOverItem = null;

    gToolbarChanged = true;
  },
  _flavourSet: null,
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/toolbaritem-id");
    }
    return this._flavourSet;
  }
}

var paletteDNDObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
  {
    aDragSession.canDrop = !gDraggingFromPalette;
  },
  onDrop: function(aEvent, aXferData, aDragSession)
  {
    var itemID = aXferData.data;
    var item = null;
    var palette = document.getElementById("palette-box");
    var toolbar = document.getElementById("cloneToolbar");
    var toolbarItem = toolbar.firstChild;
    while (toolbarItem) {
      if (toolbarItem.firstChild.id == itemID) {
        item = toolbarItem;
        break;
      }
      toolbarItem = toolbarItem.nextSibling;
    }
    if (!item)
      return;

    // We're going back in the palette now, so we have to readd the flex
    // and width which we removed when moving the item to the toolbar.
    // (These attributes help space the items properly in the palette.)
    item.setAttribute("flex", "1");
    item.setAttribute("width", "0");
    
    // XXX Now insertBefore |item| in the right place.
    gToolbarChanged = true;
  },
  _flavourSet: null,
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/toolbaritem-id");
    }
    return this._flavourSet;
  }
}

// Make sure all buttons look enabled (and that textboxes are disabled).
// Hey, you try to come up with a better name.
function cleanUpItemForAdding(aPaletteItem)
{
  aPaletteItem.removeAttribute("observes");
  aPaletteItem.removeAttribute("disabled");
  aPaletteItem.removeAttribute("type");

  if (aPaletteItem.localName == "toolbaritem" && 
      aPaletteItem.firstChild) {
    aPaletteItem.firstChild.removeAttribute("observes");
    if (aPaletteItem.firstChild.localName == "textbox")
      aPaletteItem.firstChild.setAttribute("disabled", "true");
    else
      aPaletteItem.firstChild.removeAttribute("disabled");
  }
}

// Save the changes to the toolbar and update all windows
function updateToolbar()
{
  if (!gToolbarChanged)
    return;

  var toolbar = document.getElementById("cloneToolbar");
  var node = toolbar.firstChild;
  
  var newSet = "";
  while (node) {
    newSet += node.firstChild.id;
    node = node.nextSibling;
    if (node)
      newSet += ",";
  }

  var toolbar = window.opener.document.getElementById("nav-bar");
  toolbar.setAttribute("currentset", newSet);
  window.opener.document.persist("nav-bar", "currentset");

  toolbar.rebuild();
}

// Revert back to the default set.
function resetToDefault()
{
  var toolbar = window.opener.document.getElementById("nav-bar");
  var toolbarPalette = toolbar.palette;
  var defaultSet = toolbar.getAttribute("defaultset");
  var cloneToolbar = document.getElementById("cloneToolbar");
  while (cloneToolbar.firstChild)
    cloneToolbar.removeChild(cloneToolbar.firstChild);
  var items = defaultSet.split(",");
  for (var i = 0; i < items.length; i++) {
    var item = items[i];
    if (!item) continue;

    // Attempt to locate the item within the palette's list of children.
    var paletteItem = toolbarPalette.firstChild;
    while (paletteItem) {
      var paletteID = paletteItem.getAttribute("id");
      if (paletteID == item) {
        var newItem = paletteItem.cloneNode(true);
        newItem.hidden = true;
        addItemToToolbar(newItem, cloneToolbar);
        newItem.hidden = false;
        break;
      }
        
      paletteItem = paletteItem.nextSibling;
    }
  }

  gToolbarChanged = true;
}
