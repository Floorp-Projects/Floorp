/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    let selectedNode = this.foldersTree.selectedNode;
    let selectedFolderId = PlacesUtils.getConcreteItemId(selectedNode);

    if (!PlacesUIUtils.useAsyncTransactions) {
      let transactions = [];
      for (var i=0; i < this._nodes.length; i++) {
        // Nothing to do if the node is already under the selected folder
        if (this._nodes[i].parent.itemId == selectedFolderId)
          continue;

        let txn = new PlacesMoveItemTransaction(this._nodes[i].itemId,
                                                selectedFolderId,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX);
        transactions.push(txn);
      }
      if (transactions.length != 0) {
        let txn = new PlacesAggregatedTransaction("Move Items", transactions);
        PlacesUtils.transactionManager.doTransaction(txn);
      }
      return;
    }

    PlacesTransactions.transact(function* () {
      let newParentGUID = yield PlacesUtils.promiseItemGUID(selectedFolderId);
      for (let node of this._nodes) {
        // Nothing to do if the node is already under the selected folder.
        if (node.parent.itemId == selectedFolderId)
          continue;
        yield PlacesTransactions.MoveItem({ GUID: node.bookmarkGuid
                                          , newParentGUID: newParentGUID });
      }
    }.bind(this)).then(null, Components.utils.reportError);
  },

  newFolder: function MBD_newFolder() {
    // The command is disabled when the tree is not focused
    this.foldersTree.focus();
    goDoCommand("placesCmd_new:folder");
  }
};
