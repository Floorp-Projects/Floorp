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

*/

function buildDialog()
{
  var toolbar = window.opener.document.getElementById("nav-bar");
  var cloneToolbarBox = document.getElementById("cloned-bar-container");

  var paletteBox = document.getElementById("palette-box");

  var newToolbar = toolbar.cloneNode(true);
  cloneToolbarBox.appendChild(newToolbar);

  // Make sure all buttons look enabled (and that textboxes are disabled).
  var toolbarItem = newToolbar.firstChild;
  while (toolbarItem) {
    toolbarItem.removeAttribute("observes");
    toolbarItem.removeAttribute("disabled");
    toolbarItem.removeAttribute("type");

    if (toolbarItem.localName == "toolbaritem" && 
        toolbarItem.firstChild) {
      toolbarItem.firstChild.removeAttribute("observes");
      if (toolbarItem.firstChild.localName == "textbox")
        toolbarItem.firstChild.setAttribute("disabled", "true");
      else
        toolbarItem.firstChild.removeAttribute("disabled");
    }

    toolbarItem = toolbarItem.nextSibling;
  }

  
  // Now build up a palette of items.
  var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "hbox");
  currentRow.setAttribute("class", "paletteRow");

  var rowSlot = 0;
  var rowMax = 4;

  var node = toolbar.palette.firstChild;
  while (node) {
    var paletteItem = node.cloneNode(true);
    paletteItem.removeAttribute("observes");
    paletteItem.removeAttribute("disabled");

    if (paletteItem.localName == "toolbaritem" && 
        paletteItem.firstChild) {
      paletteItem.firstChild.removeAttribute("observes");
      if (paletteItem.firstChild.localName == "textbox")
        paletteItem.firstChild.setAttribute("disabled", "true");
      else
        paletteItem.firstChild.removeAttribute("disabled");
    }

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
