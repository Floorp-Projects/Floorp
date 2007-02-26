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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com>
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
  _tm: null,

  _foldersTree: null,
  get foldersTree() {
    if (!this._foldersTree)
      this._foldersTree = document.getElementById("foldersTree");

    return this._foldersTree;
  },

  init: function() {
    this._nodes = window.arguments[0];
    this._tm = window.arguments[1];
  },

  onOK: function MBD_onOK(aEvent) {
    var selectedNode = this.foldersTree.selectedNode;
    if (!selectedNode) {
      // XXXmano: the old dialog defaults to the the "Bookmarks" root folder
      // for some reason. I'm pretty sure we don't want to that yet in Places,
      // at least not until we make that folder node visible in the tree, if we
      // ever do so
      return;
    }
    var selectedFolderID = asFolder(selectedNode).folderId;

    var transactions = [];
    for (var i=0; i < this._nodes.length; i++) {
      var parentId = asFolder(this._nodes[i].parent).folderId;

      // Nothing to do if the node is already under the selected folder
      if (parentId == selectedFolderID)
        continue;

      var nodeIndex = PlacesUtils.getIndexOfNode(this._nodes[i]);
      if (PlacesUtils.nodeIsFolder(this._nodes[i])) {
        // Disallow moving a folder into itself
        if (asFolder(this._nodes[i]).folderId != selectedFolderID) {
          transactions.push(new
            PlacesMoveFolderTransaction(asFolder(this._nodes[i]).folderId,
                                        parentId, nodeIndex,
                                        selectedFolderID, -1));
        }
      }
      else if (PlacesUtils.nodeIsBookmark(this._nodes[i])) {
        transactions.push(new
          PlacesMoveItemTransaction(this._nodes[i].bookmarkId,
                                    PlacesUtils._uri(this._nodes[i].uri),
                                    parentId, nodeIndex, selectedFolderID, -1));
      }
      else if (PlacesUtils.nodeIsSeparator(this._nodes[i])) { 
        // See makeTransaction in utils.js
        var removeTxn =
          new PlacesRemoveSeparatorTransaction(parentId, nodeIndex);
        var createTxn =
          new PlacesCreateSeparatorTransaction(selectedFolderID, -1);
        transactions.push(new
          PlacesAggregateTransaction("SeparatorMove", [removeTxn, createTxn]));
      }
    }

    if (transactions.length != 0) {
      var txn = new PlacesAggregateTransaction("Move Items", transactions);
      this._tm.doTransaction(txn);
    }
  },

  newFolder: function MBD_newFolder() {
    // The command is disabled when the tree is not focused
    this.foldersTree.focus();
    goDoCommand("placesCmd_new:folder");
  }
};
