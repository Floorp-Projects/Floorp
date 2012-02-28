/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Move Bookmarks Dialog.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com>
 *   Sungjoon Steve Won <stevewon@gmail.com>
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

var gMoveBookmarksDialog = {
  _nodes: null,

  _foldersTree: null,
  get foldersTree() {
    if (!this._foldersTree)
      this._foldersTree = document.getElementById("foldersTree");

    return this._foldersTree;
  },

  init: function() {
    this._nodes = window.arguments[0];

    this.foldersTree.place =
      "place:excludeItems=1&excludeQueries=1&excludeReadOnlyFolders=1&folder=" +
      PlacesUIUtils.allBookmarksFolderId;
  },

  onOK: function MBD_onOK(aEvent) {
    var selectedNode = this.foldersTree.selectedNode;
    NS_ASSERT(selectedNode,
              "selectedNode must be set in a single-selection tree with initial selection set");
    var selectedFolderID = PlacesUtils.getConcreteItemId(selectedNode);

    var transactions = [];
    for (var i=0; i < this._nodes.length; i++) {
      // Nothing to do if the node is already under the selected folder
      if (this._nodes[i].parent.itemId == selectedFolderID)
        continue;

      let txn = new PlacesMoveItemTransaction(this._nodes[i].itemId,
                                              selectedFolderID,
                                              PlacesUtils.bookmarks.DEFAULT_INDEX);
      transactions.push(txn);
    }

    if (transactions.length != 0) {
      let txn = new PlacesAggregatedTransaction("Move Items", transactions);
      PlacesUtils.transactionManager.doTransaction(txn);
    }
  },

  newFolder: function MBD_newFolder() {
    // The command is disabled when the tree is not focused
    this.foldersTree.focus();
    goDoCommand("placesCmd_new:folder");
  }
};
