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

var gCurrentDragOverItem = null;

function buildDialog()
{
  var toolbar = window.opener.document.getElementById("nav-bar");
  var cloneToolbarBox = document.getElementById("cloned-bar-container");
  var paletteBox = document.getElementById("palette-box");
  var enclosure;

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
    newItem.removeAttribute("observes");
    newItem.removeAttribute("disabled");
    newItem.removeAttribute("type");

    if (newItem.localName == "toolbaritem" && 
        newItem.firstChild) {
      newItem.firstChild.removeAttribute("observes");
      if (newItem.firstChild.localName == "textbox")
        newItem.firstChild.setAttribute("disabled", "true");
      else
        newItem.firstChild.removeAttribute("disabled");
    }

    enclosure = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                         "toolbarpaletteitem");
    
    // Set a draggesture handler to allow drag-rearrange within the clone toolbar.
    enclosure.setAttribute("ondraggesture", "nsDragAndDrop.startDrag(event, dragObserver)");
    enclosure.appendChild(newItem);
    newToolbar.appendChild(enclosure);
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
  while (node) {
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
    enclosure = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                         "toolbarpaletteitem");
    enclosure.setAttribute("align", "center");
    enclosure.setAttribute("pack", "center");
    enclosure.setAttribute("flex", "1");
    enclosure.setAttribute("width", "0");
    enclosure.setAttribute("minheight", "0");
    enclosure.setAttribute("minwidth", "0");
    enclosure.setAttribute("ondraggesture", "nsDragAndDrop.startDrag(event, dragObserver)");
 
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

}

var dragObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction) {
    aXferData.data = new TransferDataSet();
    var data = new TransferData();
    data.addDataForFlavour("text/unicode", aEvent.target.firstChild.id);
    aXferData.data.push(data);
  }
}

var dropObserver = {
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
    var newButtonId = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
    var toolbar = document.getElementById("cloneToolbar");
    
    // If dropping a button that's already on the toolbar, we want to move it to
    // the new location, so remove it here and we'll add it in the correct spot further down.
    var toolbarItem = toolbar.firstChild;
    while (toolbarItem) {
      if (toolbarItem.firstChild.id == newButtonId) {
        toolbar.removeChild(toolbarItem);
        break;
      }
      toolbarItem = toolbarItem.nextSibling;
    }
 
    var palette = window.opener.document.getElementById("nav-bar").palette;
    var paletteItem = palette.firstChild;
    while (paletteItem) {
      if (paletteItem.id == newButtonId)
        break;
      paletteItem = paletteItem.nextSibling;
    }

    if (!paletteItem)
      return;

    paletteItem = paletteItem.cloneNode(paletteItem);

    var enclosure = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                             "toolbarpaletteitem");
    enclosure.setAttribute("ondraggesture", "nsDragAndDrop.startDrag(event, dragObserver)");
    cleanUpItemForAdding(paletteItem);
    enclosure.appendChild(paletteItem);

    toolbar.insertBefore(enclosure, gCurrentDragOverItem);
    gCurrentDragOverItem.removeAttribute("dragactive");
    gCurrentDragOverItem = null;
  },
  _flavourSet: null,
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/unicode");
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
