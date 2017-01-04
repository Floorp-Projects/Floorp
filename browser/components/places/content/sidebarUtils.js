/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

var SidebarUtils = {
  handleTreeClick: function SU_handleTreeClick(aTree, aEvent, aGutterSelect) {
    // right-clicks are not handled here
    if (aEvent.button == 2)
      return;

    var tbo = aTree.treeBoxObject;
    var cell = tbo.getCellAt(aEvent.clientX, aEvent.clientY);

    if (cell.row == -1 || cell.childElt == "twisty")
      return;

    var mouseInGutter = false;
    if (aGutterSelect) {
      var rect = tbo.getCoordsForCellItem(cell.row, cell.col, "image");
      // getCoordsForCellItem returns the x coordinate in logical coordinates
      // (i.e., starting from the left and right sides in LTR and RTL modes,
      // respectively.)  Therefore, we make sure to exclude the blank area
      // before the tree item icon (that is, to the left or right of it in
      // LTR and RTL modes, respectively) from the click target area.
      var isRTL = window.getComputedStyle(aTree, null).direction == "rtl";
      if (isRTL)
        mouseInGutter = aEvent.clientX > rect.x;
      else
        mouseInGutter = aEvent.clientX < rect.x;
    }

    var metaKey = AppConstants.platform === "macosx" ? aEvent.metaKey
                                                     : aEvent.ctrlKey;
    var modifKey = metaKey || aEvent.shiftKey;
    var isContainer = tbo.view.isContainer(cell.row);
    var openInTabs = isContainer &&
                     (aEvent.button == 1 ||
                      (aEvent.button == 0 && modifKey)) &&
                     PlacesUtils.hasChildURIs(tbo.view.nodeForTreeIndex(cell.row));

    if (aEvent.button == 0 && isContainer && !openInTabs) {
      tbo.view.toggleOpenState(cell.row);
      return;
    } else if (!mouseInGutter && openInTabs &&
            aEvent.originalTarget.localName == "treechildren") {
      tbo.view.selection.select(cell.row);
      PlacesUIUtils.openContainerNodeInTabs(aTree.selectedNode, aEvent, aTree);
    } else if (!mouseInGutter && !isContainer &&
             aEvent.originalTarget.localName == "treechildren") {
      // Clear all other selection since we're loading a link now. We must
      // do this *before* attempting to load the link since openURL uses
      // selection as an indication of which link to load.
      tbo.view.selection.select(cell.row);
      PlacesUIUtils.openNodeWithEvent(aTree.selectedNode, aEvent, aTree);
    }
  },

  handleTreeKeyPress: function SU_handleTreeKeyPress(aEvent) {
    // XXX Bug 627901: Post Fx4, this method should take a tree parameter.
    let tree = aEvent.target;
    let node = tree.selectedNode;
    if (node) {
      if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN)
        PlacesUIUtils.openNodeWithEvent(node, aEvent, tree);
    }
  },

  /**
   * The following function displays the URL of a node that is being
   * hovered over.
   */
  handleTreeMouseMove: function SU_handleTreeMouseMove(aEvent) {
    if (aEvent.target.localName != "treechildren")
      return;

    var tree = aEvent.target.parentNode;
    var tbo = tree.treeBoxObject;
    var cell = tbo.getCellAt(aEvent.clientX, aEvent.clientY);

    // cell.row is -1 when the mouse is hovering an empty area within the tree.
    // To avoid showing a URL from a previously hovered node for a currently
    // hovered non-url node, we must clear the moused-over URL in these cases.
    if (cell.row != -1) {
      var node = tree.view.nodeForTreeIndex(cell.row);
      if (PlacesUtils.nodeIsURI(node))
        this.setMouseoverURL(node.uri);
      else
        this.setMouseoverURL("");
    } else
      this.setMouseoverURL("");
  },

  setMouseoverURL: function SU_setMouseoverURL(aURL) {
    // When the browser window is closed with an open sidebar, the sidebar
    // unload event happens after the browser's one.  In this case
    // top.XULBrowserWindow has been nullified already.
    if (top.XULBrowserWindow) {
      top.XULBrowserWindow.setOverLink(aURL, null);
    }
  }
};
