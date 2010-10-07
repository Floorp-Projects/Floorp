# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dan Mills <thunder@mozilla.com> (Ported from history-panel.js)
#   Marco Bonardo <mak77@supereva.it>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var SidebarUtils = {
  handleTreeClick: function SU_handleTreeClick(aTree, aEvent, aGutterSelect) {
    // right-clicks are not handled here
    if (aEvent.button == 2)
      return;

    var tbo = aTree.treeBoxObject;
    var row = { }, col = { }, obj = { };
    tbo.getCellAt(aEvent.clientX, aEvent.clientY, row, col, obj);

    if (row.value == -1 || obj.value == "twisty")
      return;

    var mouseInGutter = false;
    if (aGutterSelect) {
      var x = { }, y = { }, w = { }, h = { };
      tbo.getCoordsForCellItem(row.value, col.value, "image",
                               x, y, w, h);
      // getCoordsForCellItem returns the x coordinate in logical coordinates
      // (i.e., starting from the left and right sides in LTR and RTL modes,
      // respectively.)  Therefore, we make sure to exclude the blank area
      // before the tree item icon (that is, to the left or right of it in
      // LTR and RTL modes, respectively) from the click target area.
      var isRTL = window.getComputedStyle(aTree, null).direction == "rtl";
      if (isRTL)
        mouseInGutter = aEvent.clientX > x.value;
      else
        mouseInGutter = aEvent.clientX < x.value;
    }

#ifdef XP_MACOSX
    var modifKey = aEvent.metaKey || aEvent.shiftKey;
#else
    var modifKey = aEvent.ctrlKey || aEvent.shiftKey;
#endif

    var isContainer = tbo.view.isContainer(row.value);
    var openInTabs = isContainer &&
                     (aEvent.button == 1 ||
                      (aEvent.button == 0 && modifKey)) &&
                     PlacesUtils.hasChildURIs(tbo.view.nodeForTreeIndex(row.value));

    if (aEvent.button == 0 && isContainer && !openInTabs) {
      tbo.view.toggleOpenState(row.value);
      return;
    }
    else if (!mouseInGutter && openInTabs &&
            aEvent.originalTarget.localName == "treechildren") {
      tbo.view.selection.select(row.value);
      PlacesUIUtils.openContainerNodeInTabs(aTree.selectedNode, aEvent);
    }
    else if (!mouseInGutter && !isContainer &&
             aEvent.originalTarget.localName == "treechildren") {
      // Clear all other selection since we're loading a link now. We must
      // do this *before* attempting to load the link since openURL uses
      // selection as an indication of which link to load.
      tbo.view.selection.select(row.value);
      PlacesUIUtils.openNodeWithEvent(aTree.selectedNode, aEvent);
    }
  },

  handleTreeKeyPress: function SU_handleTreeKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN)
      PlacesUIUtils.openNodeWithEvent(aEvent.target.selectedNode, aEvent);
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
    var row = { }, col = { }, obj = { };
    tbo.getCellAt(aEvent.clientX, aEvent.clientY, row, col, obj);

    // row.value is -1 when the mouse is hovering an empty area within the tree.
    // To avoid showing a URL from a previously hovered node for a currently
    // hovered non-url node, we must clear the moused-over URL in these cases.
    if (row.value != -1) {
      var node = tree.view.nodeForTreeIndex(row.value);
      if (PlacesUtils.nodeIsURI(node))
        this.setMouseoverURL(node.uri);
      else
        this.setMouseoverURL("");
    }
    else
      this.setMouseoverURL("");
  },

  setMouseoverURL: function SU_setMouseoverURL(aURL) {
    window.top.XULBrowserWindow.setOverLink(aURL, null);
  }
};
