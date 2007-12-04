/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Command Controller.
 *
 * The Initial Developer of the Original Code is Google Inc.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sungjoon Steve Won <stevewon@gmail.com> (Original Author)
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

const loadInSidebarAnno = "bookmarkProperties/loadInSidebar";
const descriptionAnno = "bookmarkProperties/description";
const CLASS_ID = Components.ID("c0844a84-5a12-4808-80a8-809cb002bb4f");
const CONTRACT_ID = "@mozilla.org/browser/placesTransactionsService;1";

var loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"].
             getService(Components.interfaces.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://global/content/debug.js");
loader.loadSubScript("chrome://browser/content/places/utils.js");

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// The minimum amount of transactions we should tell our observers to begin
// batching (rather than letting them do incremental drawing).
const MIN_TRANSACTIONS_FOR_BATCH = 5;  

function placesTransactionsService() {
  this.mTransactionManager = Cc["@mozilla.org/transactionmanager;1"].
                             createInstance(Ci.nsITransactionManager);
}

placesTransactionsService.prototype = {
  classDescription: "Places Transaction Manager",
  classID: CLASS_ID,
  contractID: CONTRACT_ID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPlacesTransactionsService,
                                         Ci.nsITransactionManager]),

  aggregateTransactions: function placesAggrTransactions(name, transactions) {
    return new placesAggregateTransactions(name, transactions);
  },

  createFolder: function placesCrtFldr(aName, aContainer, aIndex,
                                       aAnnotations, aChildItemsTransactions) {
     return new placesCreateFolderTransactions(aName, aContainer, aIndex,
                                               aAnnotations, aChildItemsTransactions);
  },

  createItem: function placesCrtItem(aURI, aContainer, aIndex, aTitle,
                                     aKeyword, aAnnotations, aChildTransactions) {
    return new placesCreateItemTransactions(aURI, aContainer, aIndex, aTitle,
                                            aKeyword, aAnnotations, aChildTransactions);
  },

  createSeparator: function placesCrtSpr(aContainer, aIndex) {
    return new placesCreateSeparatorTransactions(aContainer, aIndex);
  },

  createLivemark: function placesCrtLivemark(aFeedURI, aSiteURI, aName,
                                             aContainer, aIndex, aAnnotations) {
    return new placesCreateLivemarkTransactions(aFeedURI, aSiteURI, aName,
                                                aContainer, aIndex, aAnnotations);
  },

  moveItem: function placesMvItem(aItemId, aNewContainer, aNewIndex) {
    return new placesMoveItemTransactions(aItemId, aNewContainer, aNewIndex);
  },

  removeItem: function placesRmItem(id) {
    return new placesRemoveItemTransaction(id);
  },

  editItemTitle: function placesEditItmTitle(id, newTitle) {
    return new placesEditItemTitleTransactions(id, newTitle);
  },

  editBookmarkURI: function placesEditBkmkURI(aBookmarkId, aNewURI) {
    return new placesEditBookmarkURITransactions(aBookmarkId, aNewURI);
  },

  setLoadInSidebar: function placesSetLdInSdbar(aBookmarkId, aLoadInSidebar) {
    return new placesSetLoadInSidebarTransactions(aBookmarkId, aLoadInSidebar);
  },

  editItemDescription: function placesEditItmDesc(aItemId, aDescription) {
    return new placesEditItemDescriptionTransactions(aItemId, aDescription);
  },

  editBookmarkKeyword: function placesEditBkmkKwd(id, newKeyword) {
    return new placesEditBookmarkKeywordTransactions(id, newKeyword);
  },

  editURIPostData: function placesEditURIPdata(aURI, aPostData) {
    return new placesEditURIPostDataTransactions(aURI, aPostData);
  },

  editLivemarkSiteURI: function placesEditLvmkSiteURI(folderId, uri) {
    return new placesEditLivemarkSiteURITransactions(folderId, uri);
  },

  editLivemarkFeedURI: function placesEditLvmkFeedURI(folderId, uri) {
    return new placesEditLivemarkFeedURITransactions(folderId, uri);
  },

  editBookmarkMicrosummary: function placesEditBkmkMicrosummary(aID, newMicrosummary) {
    return new placesEditBookmarkMicrosummaryTransactions(aID, newMicrosummary);
  },

  sortFolderByName: function placesSortFldrByName(aFolderId, aFolderIndex) {
   return new placesSortFolderByNameTransactions(aFolderId, aFolderIndex);
  },

  // Update commands in the undo group of the active window
  // commands in inactive windows will are updated on-focus
  _updateCommands: function() {
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
    var win = wm.getMostRecentWindow(null);
    if (win)
      win.updateCommands("undo");
  },

  // nsITransactionManager
  doTransaction: function doTransaction(txn) {
    this.mTransactionManager.doTransaction(txn);
    this._updateCommands();
  },

  undoTransaction: function undoTransaction() {
    this.mTransactionManager.undoTransaction();
    this._updateCommands();
  },

  redoTransaction: function redoTransaction() {
    this.mTransactionManager.redoTransaction();
    this._updateCommands();
  },

  clear: function() this.mTransactionManager.clear(),
  beginBatch: function() this.mTransactionManager.beginBatch(),
  endBatch: function() this.mTransactionManager.endBatch(),

  get numberOfUndoItems() {
    return this.mTransactionManager.numberOfUndoItems;
  },

  get numberOfRedoItems() {
    return this.mTransactionManager.numberOfRedoItems;
  },

  get maxTransactionCount() {
    return this.mTransactionManager.maxTransactionCount;
  },
  set maxTransactionCount(val) {
    return this.mTransactionManager.maxTransactionCount = val;
  },

  peekUndoStack: function() this.mTransactionManager.peekUndoStack(),
  peekRedoStack: function() this.mTransactionManager.peekRedoStack(),
  getUndoStack: function() this.mTransactionManager.getUndoStack(),
  getRedoStack: function() this.mTransactionManager.getRedoStack(),
  AddListener: function(l) this.mTransactionManager.AddListener(l),
  RemoveListener: function(l) this.mTransactionManager.RemoveListener(l)
};

/**
 * Method and utility stubs for Places Edit Transactions
 */
function placesBaseTransaction() {
}

placesBaseTransaction.prototype = {
  // for child-transactions
  get wrappedJSObject() {
    return this;
  },

  // nsITransaction
  redoTransaction: function PIT_redoTransaction() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  get isTransient() {
    return false;
  },

  merge: function mergeFunc(transaction) {
    return false;
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITransaction]),
};

function placesAggregateTransactions(name, transactions) {
  this._transactions = transactions;
  this._name = name;
  this.container = -1;
  this.redoTransaction = this.doTransaction;
}

placesAggregateTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PAT_doTransaction() {
    if (this._transactions.length >= MIN_TRANSACTIONS_FOR_BATCH) {
      var callback = {
        _self: this,
        runBatched: function() {
          this._self.commit(false);
        }
      };
      PlacesUtils.bookmarks.runInBatchMode(callback, null);
    }
    else
      this.commit(false);
  },

  undoTransaction: function PAT_undoTransaction() {
    if (this._transactions.length >= MIN_TRANSACTIONS_FOR_BATCH) {
      var callback = {
        _self: this,
        runBatched: function() {
          this._self.commit(true);
        }
      };
      PlacesUtils.bookmarks.runInBatchMode(callback, null);
    }
    else
      this.commit(true);
  },

  commit: function PAT_commit(aUndo) {
    for (var i=0; i < this._transactions.length; ++i) {
      var txn = this._transactions[i];
      if (this.container > -1) 
        txn.wrappedJSObject.container = this.container;
      if (aUndo)
        txn.undoTransaction();
      else
        txn.doTransaction();
    }
  }
};

function placesCreateFolderTransactions(aName, aContainer, aIndex,
                                        aAnnotations,
                                        aChildItemsTransactions) {
  this._name = aName;
  this._container = aContainer;
  this._index = typeof(aIndex) == "number" ? aIndex : -1;
  this._annotations = aAnnotations;
  this._id = null;
  this._childItemsTransactions = aChildItemsTransactions || [];
  this.redoTransaction = this.doTransaction;
}

placesCreateFolderTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  // childItemsTransaction support
  get container() { return this._container; },
  set container(val) { return this._container = val; },

  doTransaction: function PCFT_doTransaction() {
    this._id = PlacesUtils.bookmarks.createFolder(this._container, 
                                                  this._name, this._index);
    if (this._annotations && this._annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this._id, this._annotations);

    for (var i = 0; i < this._childItemsTransactions.length; ++i) {
      var txn = this._childItemsTransactions[i];
      txn.wrappedJSObject.container = this._id;
      txn.doTransaction();
    }
  },

  undoTransaction: function PCFT_undoTransaction() {
    PlacesUtils.bookmarks.removeFolder(this._id);
    for (var i = 0; i < this._childItemsTransactions.length; ++i) {
      var txn = this.childItemsTransactions[i];
      txn.undoTransaction();
    }
  }
};

function placesCreateItemTransactions(aURI, aContainer, aIndex, aTitle,
                                      aKeyword, aAnnotations,
                                      aChildTransactions) {
  this._uri = aURI;
  this._container = aContainer;
  this._index = typeof(aIndex) == "number" ? aIndex : -1;
  this._title = aTitle;
  this._keyword = aKeyword;
  this._annotations = aAnnotations;
  this._childTransactions = aChildTransactions || [];
  this.redoTransaction = this.doTransaction;
}

placesCreateItemTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  // childItemsTransactions support for the create-folder transaction
  get container() { return this._container; },
  set container(val) { return this._container = val; },

  doTransaction: function PCIT_doTransaction() {
    this._id = PlacesUtils.bookmarks.insertBookmark(this.container, this._uri,
                                                    this._index, this._title);
    if (this._keyword)
      PlacesUtils.bookmarks.setKeywordForBookmark(this._id, this._keyword);
    if (this._annotations && this._annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this._id, this._annotations);

    for (var i = 0; i < this._childTransactions.length; ++i) {
      var txn = this._childTransactions[i];
      txn.wrappedJSObject.id = this._id;
      txn.doTransaction();
    }
  },

  undoTransaction: function PCIT_undoTransaction() {
    PlacesUtils.bookmarks.removeItem(this._id);
    for (var i = 0; i < this._childTransactions.length; ++i) {
      var txn = this._childTransactions[i];
      txn.undoTransaction();
    }
  }
};

function placesCreateSeparatorTransactions(aContainer, aIndex) {
  this._container = aContainer;
  this._index = typeof(aIndex) == "number" ? aIndex : -1;
  this._id = null;
}

placesCreateSeparatorTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  // childItemsTransaction support
  get container() { return this._container; },
  set container(val) { return this._container = val;clear },

  doTransaction: function PCST_doTransaction() {
    this._id = PlacesUtils.bookmarks
                          .insertSeparator(this.container, this._index);
  },

  undoTransaction: function PCST_undoTransaction() {
    PlacesUtils.bookmarks.removeChildAt(this.container, this._index);
  }
};

function placesCreateLivemarkTransactions(aFeedURI, aSiteURI, aName,
                                          aContainer, aIndex,
                                          aAnnotations) {
  this._feedURI = aFeedURI;
  this._siteURI = aSiteURI;
  this._name = aName;
  this._container = aContainer;
  this._index = typeof(aIndex) == "number" ? aIndex : -1;
  this._annotations = aAnnotations;
}

placesCreateLivemarkTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  // childItemsTransaction support
  get container() { return this._container; },
  set container(val) { return this._container = val; },

  doTransaction: function PCLT_doTransaction() {
    this._id = PlacesUtils.livemarks.createLivemark(this._container, this._name,
                                                    this._siteURI, this._feedURI,
                                                    this._index);
    if (this._annotations && this._annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this._id, this._annotations);
  },

  undoTransaction: function PCLT_undoTransaction() {
    PlacesUtils.bookmarks.removeFolder(this._id);
  }
};

function placesMoveItemTransactions(aItemId, aNewContainer, aNewIndex) {
  NS_ASSERT(aNewIndex >= -1, "invalid insertion index");
  this._id = aItemId;
  this._oldContainer = PlacesUtils.bookmarks.getFolderIdForItem(this._id);
  this._oldIndex = PlacesUtils.bookmarks.getItemIndex(this._id);
  NS_ASSERT(this._oldContainer > 0 && this._oldIndex >= 0, "invalid item");
  this._newContainer = aNewContainer;
  this._newIndex = aNewIndex;
  this.redoTransaction = this.doTransaction;
}

placesMoveItemTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PMIT_doTransaction() {
    PlacesUtils.bookmarks.moveItem(this._id, this._newContainer, this._newIndex);
  },

  undoTransaction: function PMIT_undoTransaction() {
    PlacesUtils.bookmarks.moveItem(this._id, this._oldContainer, this._oldIndex);
  }
};

function placesRemoveItemTransaction(aItemId) {
  this.redoTransaction = this.doTransaction;
  this._id = aItemId;
  this._itemType = PlacesUtils.bookmarks.getItemType(this._id);
  if (this._itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
    this._transactions = [];
    this._removeTxn = PlacesUtils.bookmarks
                                 .getRemoveFolderTransaction(this._id);
  }
}

placesRemoveItemTransaction.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PRIT_doTransaction() {
    this._oldContainer = PlacesUtils.bookmarks.getFolderIdForItem(this._id);
    this._oldIndex = PlacesUtils.bookmarks.getItemIndex(this._id);
    this._title = PlacesUtils.bookmarks.getItemTitle(this._id);
    this._annotations = PlacesUtils.getAnnotationsForItem(this._id);

    if (this._itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      this._saveFolderContents();

      // Remove children backwards to preserve parent-child relationships.
      for (var i = this._transactions.length - 1; i >= 0; --i)
        this._transactions[i].doTransaction();
    
      // Remove this folder itself. 
      this._removeTxn.doTransaction();
    }
    else {
      if (this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK)
        this._uri = PlacesUtils.bookmarks.getBookmarkURI(this._id);
      PlacesUtils.bookmarks.removeItem(this._id);
    }
  },

  undoTransaction: function PRIT_undoTransaction() {
    if (this._itemType == Ci.nsINavBookmarksService.TYPE_BOOKMARK) {
      this._id = PlacesUtils.bookmarks.insertBookmark(this._oldContainer,
                                                      this._uri,
                                                      this._oldIndex,
                                                      this._title);
    }
    else if (this._itemType == Ci.nsINavBookmarksService.TYPE_FOLDER) {
      this._removeTxn.undoTransaction();
      // Create children forwards to preserve parent-child relationships.
      for (var i = 0; i < this._transactions.length; ++i)
        this._transactions[i].undoTransaction();
    }
    else // TYPE_SEPARATOR
      PlacesUtils.bookmarks.insertSeparator(this._oldContainer, this._oldIndex);

    if (this._annotations.length > 0)
      PlacesUtils.setAnnotationsForItem(this._id, this._annotations);
  },

  /**
  * Create a flat, ordered list of transactions for a depth-first recreation
  * of items within this folder.
  */
  _saveFolderContents: function PRIT__saveFolderContents() {
    this._transactions = [];
    var contents =
      PlacesUtils.getFolderContents(this._id, false, false).root;
    for (var i = 0; i < contents.childCount; ++i) {
      this._transactions
          .push(new placesRemoveItemTransaction(contents.getChild(i).itemId));
    }
  }
};

function placesEditItemTitleTransactions(id, newTitle) {
  this._id = id;
  this._newTitle = newTitle;
  this._oldTitle = "";
  this.redoTransaction = this.doTransaction;
}

placesEditItemTitleTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PEITT_doTransaction() {
    this._oldTitle = PlacesUtils.bookmarks.getItemTitle(this._id);
    PlacesUtils.bookmarks.setItemTitle(this._id, this._newTitle);
  },

  undoTransaction: function PEITT_undoTransaction() {
    PlacesUtils.bookmarks.setItemTitle(this._id, this._oldTitle);
  }
};

function placesEditBookmarkURITransactions(aBookmarkId, aNewURI) {
  this._id = aBookmarkId;
  this._newURI = aNewURI;
  this.redoTransaction = this.doTransaction;
}

placesEditBookmarkURITransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PEBUT_doTransaction() {
    this._oldURI = PlacesUtils.bookmarks.getBookmarkURI(this._id);
    PlacesUtils.bookmarks.changeBookmarkURI(this._id, this._newURI);
  },

  undoTransaction: function PEBUT_undoTransaction() {
    PlacesUtils.bookmarks.changeBookmarkURI(this._id, this._oldURI);
  }
};

function placesSetLoadInSidebarTransactions(aBookmarkId, aLoadInSidebar) {
  this.id = aBookmarkId;
  this._loadInSidebar = aLoadInSidebar;
  this.redoTransaction = this.doTransaction;
}

placesSetLoadInSidebarTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  _anno: {
    name: loadInSidebarAnno,
    type: Ci.nsIAnnotationService.TYPE_INT32,
    value: 1,
    flags: 0,
    expires: Ci.nsIAnnotationService.EXPIRE_NEVER
  },

  doTransaction: function PSLIST_doTransaction() {
    this._wasSet = PlacesUtils.annotations.itemHasAnnotation(this.id, this._anno.name);
    if (this._loadInSidebar) {
      PlacesUtils.setAnnotationsForItem(this.id, [this._anno]);
    }
    else {
      try {
        PlacesUtils.annotations.removeItemAnnotation(this.id, this._anno.name);
      } catch(ex) { }
    }
  },

  undoTransaction: function PSLIST_undoTransaction() {
    if (this._wasSet != this._loadInSidebar) {
      this._loadInSidebar = !this._loadInSidebar;
      this.doTransaction();
    }
  }
};

function placesEditItemDescriptionTransactions(aItemId, aDescription) {
  this.id = aItemId;
  this._newDescription = aDescription;
  this.redoTransaction = this.doTransaction;
}

placesEditItemDescriptionTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  _oldDescription: "",

  doTransaction: function PSLIST_doTransaction() {
    const annos = PlacesUtils.annotations;
    if (annos.itemHasAnnotation(this.id, descriptionAnno))
      this._oldDescription = annos.getItemAnnotation(this.id, descriptionAnno);

    if (this._newDescription) {
      annos.setItemAnnotation(this.id, descriptionAnno,
                              this._newDescription, 0,
                              annos.EXPIRE_NEVER);
    }
    else if (this._oldDescription)
      annos.removeItemAnnotation(this.id, descriptionAnno);
  },

  undoTransaction: function PSLIST_undoTransaction() {
    const annos = PlacesUtils.annotations;
    if (this._oldDescription) {
      annos.setItemAnnotationString(this.id, descriptionAnno,
                                    this._oldDescription, 0,
                                    annos.EXPIRE_NEVER);
    }
    else if (annos.itemHasAnnotation(this.id, descriptionAnno))
      annos.removeItemAnnotation(this.id, descriptionAnno);
  }
};

function placesEditBookmarkKeywordTransactions(id, newKeyword) {
  this.id = id;
  this._newKeyword = newKeyword;
  this._oldKeyword = "";
  this.redoTransaction = this.doTransaction;
}

placesEditBookmarkKeywordTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PEBKT_doTransaction() {
    this._oldKeyword = PlacesUtils.bookmarks.getKeywordForBookmark(this.id);
    PlacesUtils.bookmarks.setKeywordForBookmark(this.id, this._newKeyword);
  },

  undoTransaction: function PEBKT_undoTransaction() {
    PlacesUtils.bookmarks.setKeywordForBookmark(this.id, this._oldKeyword);
  }
};

function placesEditURIPostDataTransactions(aURI, aPostData) {
  this._uri = aURI;
  this._newPostData = aPostData;
  this._oldPostData = null;
  this.redoTransaction = this.doTransaction;
}

placesEditURIPostDataTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PEUPDT_doTransaction() {
    this._oldPostData = PlacesUtils.getPostDataForURI(this._uri);
    PlacesUtils.setPostDataForURI(this._uri, this._newPostData);
  },

  undoTransaction: function PEUPDT_undoTransaction() {
    PlacesUtils.setPostDataForURI(this._uri, this._oldPostData);
  }
};

function placesEditLivemarkSiteURITransactions(folderId, uri) {
  this._folderId = folderId;
  this._newURI = uri;
  this._oldURI = null;
  this.redoTransaction = this.doTransaction;
}

placesEditLivemarkSiteURITransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PELSUT_doTransaction() {
    this._oldURI = PlacesUtils.livemarks.getSiteURI(this._folderId);
    PlacesUtils.livemarks.setSiteURI(this._folderId, this._newURI);
  },

  undoTransaction: function PELSUT_undoTransaction() {
    PlacesUtils.livemarks.setSiteURI(this._folderId, this._oldURI);
  }
};

function placesEditLivemarkFeedURITransactions(folderId, uri) {
  this._folderId = folderId;
  this._newURI = uri;
  this._oldURI = null;
  this.redoTransaction = this.doTransaction;
}

placesEditLivemarkFeedURITransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PELFUT_doTransaction() {
    this._oldURI = PlacesUtils.livemarks.getFeedURI(this._folderId);
    PlacesUtils.livemarks.setFeedURI(this._folderId, this._newURI);
    PlacesUtils.livemarks.reloadLivemarkFolder(this._folderId);
  },

  undoTransaction: function PELFUT_undoTransaction() {
    PlacesUtils.livemarks.setFeedURI(this._folderId, this._oldURI);
    PlacesUtils.livemarks.reloadLivemarkFolder(this._folderId);
  }
};

function placesEditBookmarkMicrosummaryTransactions(aID, newMicrosummary) {
  this.id = aID;
  this._mss = Cc["@mozilla.org/microsummary/service;1"].
              getService(Ci.nsIMicrosummaryService);
  this._newMicrosummary = newMicrosummary;
  this._oldMicrosummary = null;
  this.redoTransaction = this.doTransaction;
}

placesEditBookmarkMicrosummaryTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PEBMT_doTransaction() {
    this._oldMicrosummary = this._mss.getMicrosummary(this.id);
    if (this._newMicrosummary)
      this._mss.setMicrosummary(this.id, this._newMicrosummary);
    else
      this._mss.removeMicrosummary(this.id);
  },

  undoTransaction: function PEBMT_undoTransaction() {
    if (this._oldMicrosummary)
      this._mss.setMicrosummary(this.id, this._oldMicrosummary);
    else
      this._mss.removeMicrosummary(this.id);
  }
};

function placesSortFolderByNameTransactions(aFolderId, aFolderIndex) {
  this._folderId = aFolderId;
  this._folderIndex = aFolderIndex;
  this._oldOrder = null,
  this.redoTransaction = this.doTransaction;
}

placesSortFolderByNameTransactions.prototype = {
  __proto__: placesBaseTransaction.prototype,

  doTransaction: function PSSFBN_doTransaction() {
    this._oldOrder = [];

    var contents = PlacesUtils.getFolderContents(this._folderId, false, false).root;
    var count = contents.childCount;

    // sort between separators
    var newOrder = []; 
    var preSep = []; // temporary array for sorting each group of items
    var sortingMethod =
      function (a, b) { return a.title.localeCompare(b.title); };

    for (var i = 0; i < count; ++i) {
      var item = contents.getChild(i);
      this._oldOrder[item.itemId] = i;
      if (PlacesUtils.nodeIsSeparator(item)) {
        if (preSep.length > 0) {
          preSep.sort(sortingMethod);
          newOrder = newOrder.concat(preSep);
          preSep.splice(0);
        }
        newOrder.push(item);
      }
      else
        preSep.push(item);
    }
    if (preSep.length > 0) {
      preSep.sort(sortingMethod);
      newOrder = newOrder.concat(preSep);
    }

    // set the nex indexs
    for (var i = 0; i < count; ++i)
      PlacesUtils.bookmarks.setItemIndex(newOrder[i].itemId, i);
  },

  undoTransaction: function PSSFBN_undoTransaction() {
    for (item in this._oldOrder)
      PlacesUtils.bookmarks.setItemIndex(item, this._oldOrder[item]);
  }
};

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([placesTransactionsService]);
}
