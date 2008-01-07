/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is example Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Sungjoon Steve Won <stevewon@gmail.com> (Original Author)
 *  Asaf Romano <mano@mozilla.com>
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

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get livemark service
try {
  var lmsvc = Cc["@mozilla.org/browser/livemark-service;2"].getService(Ci.nsILivemarkService);
} catch(ex) {
  do_throw("Could not get livemark-service\n");
} 

// Get microsummary service
try {
  var mss = Cc["@mozilla.org/microsummary/service;1"].getService(Ci.nsIMicrosummaryService);
} catch(ex) {
  do_throw("Could not get microsummary-service\n");
} 

// Get Places Transaction Manager Service
try {
    var ptSvc =
      Cc["@mozilla.org/browser/placesTransactionsService;1"].
        getService(Ci.nsIPlacesTransactionsService);
} catch(ex) {
  do_throw("Could not get Places Transactions Service\n");
}

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  onItemAdded: function(id, folder, index) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
  },
  onItemRemoved: function(id, folder, index) {
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  onItemChanged: function(id, property, isAnnotationProperty, value) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
  },
  onItemVisited: function(id, visitID, time) {
    this._itemVisitedId = id;
    this._itemVisitedVistId = visitID;
    this._itemVisitedTime = time;
  },
  onItemMoved: function(id, oldParent, oldIndex, newParent, newIndex) {
    this._itemMovedId = id
    this._itemMovedOldParent = oldParent;
    this._itemMovedOldIndex = oldIndex;
    this._itemMovedNewParent = newParent;
    this._itemMovedNewIndex = newIndex;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
bmsvc.addObserver(observer, false);

// index at which items should begin
var bmStartIndex = 0;

// main
function run_test() {
  // get bookmarks root index
  var root = bmsvc.bookmarksMenuFolder;

  const DESCRIPTION_ANNO = "bookmarkProperties/description";
  var testDescription = "this is my test description";
  var annotationService = Cc["@mozilla.org/browser/annotation-service;1"].
                          getService(Ci.nsIAnnotationService);

  //Test creating a folder with a description
  var annos = [{ name: DESCRIPTION_ANNO,
                 type: Ci.nsIAnnotationService.TYPE_STRING,
                flags: 0,
                value: testDescription,
              expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];
  var txn1 = ptSvc.createFolder("Testing folder", root, bmStartIndex, annos);
  txn1.doTransaction();
  var folderId = bmsvc.getChildFolder(root, "Testing folder");
  do_check_eq(testDescription, 
              annotationService.getItemAnnotation(folderId, DESCRIPTION_ANNO));
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, folderId);
  txn1.undoTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  // Test creating an item
  // Create to Root
  var txn2 = ptSvc.createItem(uri("http://www.example.com"), root, bmStartIndex, "Testing1");
  ptSvc.doTransaction(txn2); //Also testing doTransaction
  var b = (bmsvc.getBookmarkIdsForURI(uri("http://www.example.com"), {}))[0];
  do_check_eq(observer._itemAddedId, b);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_true(bmsvc.isBookmarked(uri("http://www.example.com")));
  txn2.undoTransaction();
  do_check_eq(observer._itemRemovedId, b);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  // Create to a folder
  var txn2a = ptSvc.createFolder("Folder", root, bmStartIndex);
  ptSvc.doTransaction(txn2a);
  var fldrId = bmsvc.getChildFolder(root, "Folder");
  var txn2b = ptSvc.createItem(uri("http://www.example2.com"), fldrId, bmStartIndex, "Testing1b");
  ptSvc.doTransaction(txn2b);
  var b2 = (bmsvc.getBookmarkIdsForURI(uri("http://www.example2.com"), {}))[0];
  do_check_eq(observer._itemAddedId, b2);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_true(bmsvc.isBookmarked(uri("http://www.example2.com")));
  txn2b.undoTransaction();
  do_check_eq(observer._itemRemovedId, b2);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  // Testing moving an item
  ptSvc.doTransaction(ptSvc.createItem(uri("http://www.example3.com"), root, -1, "Testing2"));
  ptSvc.doTransaction(ptSvc.createItem(uri("http://www.example3.com"), root, -1, "Testing3"));   
  ptSvc.doTransaction(ptSvc.createItem(uri("http://www.example3.com"), fldrId, -1, "Testing4"));
  var bkmkIds = bmsvc.getBookmarkIdsForURI(uri("http://www.example3.com"), {});
  bkmkIds.sort();
  var bkmk1Id = bkmkIds[0];
  var bkmk2Id = bkmkIds[1];
  var bkmk3Id = bkmkIds[2];
  var txn3 = ptSvc.moveItem(bkmk1Id, root, -1);
  txn3.doTransaction();

  // Moving items between the same folder
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 2);
  txn3.undoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 2);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 1);

  // Moving items between different folders
  var txn3b = ptSvc.moveItem(bkmk1Id, fldrId, -1);
  txn3b.doTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, fldrId);
  do_check_eq(observer._itemMovedNewIndex, 1);
  txn3.undoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, fldrId);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 1);

  // Test Removing a Folder
  ptSvc.doTransaction(ptSvc.createFolder("Folder2", root, -1));
  var fldrId2 = bmsvc.getChildFolder(root, "Folder2");
  var txn4 = ptSvc.removeItem(fldrId2);
  txn4.doTransaction();
  do_check_eq(observer._itemRemovedId, fldrId2);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 3);
  txn4.undoTransaction();
  do_check_eq(observer._itemAddedId, fldrId2);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 3);

  // Test removing an item
  var txn5 = ptSvc.removeItem(bkmk2Id);
  txn5.doTransaction();
  do_check_eq(observer._itemRemovedId, bkmk2Id);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 1);
  txn5.undoTransaction();

  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 1);

  // Test creating a separator
  var txn6 = ptSvc.createSeparator(root, 1);
  txn6.doTransaction();
  var sepId = observer._itemAddedId;
  do_check_eq(observer._itemAddedIndex, 1);
  do_check_eq(observer._itemAddedParent, root);
  txn6.undoTransaction();
  do_check_eq(observer._itemRemovedId, sepId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 1);

  // Test removing a separator
  ptSvc.doTransaction(ptSvc.createSeparator(root, 1));
  var sepId2 = observer._itemAddedId;
  var txn7 = ptSvc.removeItem(sepId2);
  txn7.doTransaction();
  do_check_eq(observer._itemRemovedId, sepId2);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 1);
  txn7.undoTransaction();
  do_check_eq(observer._itemAddedId, sepId2); //New separator created
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 1);

  // Test editing item title
  var txn8 = ptSvc.editItemTitle(bkmk1Id, "Testing2_mod");
  txn8.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id); 
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Testing2_mod");
  txn8.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id); 
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Testing2");

  // Test editing item uri
  var txn9 = ptSvc.editBookmarkURI(bkmk1Id, uri("http://newuri.com"));
  txn9.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://newuri.com/");
  txn9.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://www.example3.com/");
  
  // Test edit item description
  var txn10 = ptSvc.editItemDescription(bkmk1Id, "Description1");
  txn10.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "bookmarkProperties/description");

  // Testing edit keyword
  var txn11 = ptSvc.editBookmarkKeyword(bkmk1Id, "kw1");
  txn11.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "keyword");
  do_check_eq(observer._itemChangedValue, "kw1"); 
  txn11.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "keyword");
  do_check_eq(observer._itemChangedValue, ""); 

  var txn12 = ptSvc.createLivemark(uri("http://feeduri.com"), uri("http://siteuri.com"), "Livemark1", root);
  txn12.doTransaction();

  // Funky stuff going on here.
  // In placesCreateLivemarkTxn, livemarks.createLivemark actually returns observer._itemAddedId -1
  // instead of observer._itemAddedId. Check w. someone.
  do_check_true(lmsvc.isLivemark(observer._itemAddedId-1));
  do_check_eq(lmsvc.getSiteURI(observer._itemAddedId-1).spec, "http://siteuri.com/");
  do_check_eq(lmsvc.getFeedURI(observer._itemAddedId-1).spec, "http://feeduri.com/");
  var lvmkId = observer._itemAddedId-1;

  // editLivemarkSiteURI
  var txn13 = ptSvc.editLivemarkSiteURI(lvmkId, uri("http://NEWsiteuri.com/"));
  txn13.doTransaction();
  do_check_eq(observer._itemChangedId, lvmkId);
  do_check_eq(observer._itemChangedProperty, "livemark/siteURI");

  txn13.undoTransaction();
  do_check_eq(observer._itemChangedId, lvmkId);
  do_check_eq(observer._itemChangedProperty, "livemark/siteURI");
  do_check_eq(observer._itemChangedValue, "");

  // editLivemarkFeedURI
  var txn14 = ptSvc.editLivemarkFeedURI(lvmkId, uri("http://NEWfeeduri.com/"));
  txn14.doTransaction();
  do_check_eq(observer._itemChangedId, lvmkId);
  do_check_eq(observer._itemChangedProperty, "livemark/feedURI");

  txn14.undoTransaction();
  do_check_eq(observer._itemChangedId, lvmkId);
  do_check_eq(observer._itemChangedProperty, "livemark/feedURI");
  do_check_eq(observer._itemChangedValue, "");

  // Test setLoadInSidebar
  var txn16 = ptSvc.setLoadInSidebar(bkmk1Id, true);
  txn16.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "bookmarkProperties/loadInSidebar");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);
  txn16.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "bookmarkProperties/loadInSidebar");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  // sortFolderByName
  ptSvc.doTransaction(ptSvc.createFolder("Sorting folder", root, bmStartIndex, [], null));
  var srtFldId = bmsvc.getChildFolder(root, "Sorting folder");
  ptSvc.doTransaction(ptSvc.createItem(uri("http://www.sortingtest.com"), srtFldId, -1, "c"));
  ptSvc.doTransaction(ptSvc.createItem(uri("http://www.sortingtest.com"), srtFldId, -1, "b"));   
  ptSvc.doTransaction(ptSvc.createItem(uri("http://www.sortingtest.com"), srtFldId, -1, "a"));
  var b = bmsvc.getBookmarkIdsForURI(uri("http://www.sortingtest.com"), {});
  b.sort();
  var b1 = b[0];
  var b2 = b[1];
  var b3 = b[2];
  do_check_eq(0, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(2, bmsvc.getItemIndex(b3));
  var txn17 = ptSvc.sortFolderByName(srtFldId, 1);
  txn17.doTransaction();
  do_check_eq(2, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(0, bmsvc.getItemIndex(b3));
  txn17.undoTransaction();
  do_check_eq(0, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(2, bmsvc.getItemIndex(b3));

  // editBookmarkMicrosummary
  var tmpMs = mss.createMicrosummary(uri("http://testmicro.com"), 
                                     uri("http://dietrich.ganx4.com/mozilla/test-microsummary.xml"));
  ptSvc.doTransaction(
  ptSvc.createItem(uri("http://dietrich.ganx4.com/mozilla/test-microsummary-content.php"),
                   root, -1, "micro test", null, null, null));
  var bId = (bmsvc.getBookmarkIdsForURI(uri("http://dietrich.ganx4.com/mozilla/test-microsummary-content.php"),{}))[0];
  do_check_true(!mss.hasMicrosummary(bId));
  var txn18 = ptSvc.editBookmarkMicrosummary(bId, tmpMs);
  txn18.doTransaction();
  do_check_eq(observer._itemChangedId, bId);
  do_check_true(mss.hasMicrosummary(bId));
  txn18.undoTransaction();
  do_check_eq(observer._itemChangedId, bId);
  do_check_true(!mss.hasMicrosummary(bId));

  // Testing edit Post Data
  const POST_DATA_ANNO = "bookmarkProperties/POSTData";
  var postData = "foo";
  var postDataURI = uri("http://foo.com");
  ptSvc.doTransaction(
    ptSvc.createItem(postDataURI, root, -1, "postdata test", null, null, null));
  var postDataId = (bmsvc.getBookmarkIdsForURI(postDataURI,{}))[0];
  var postDataTxn = ptSvc.editBookmarkPostData(postDataId, postData);
  postDataTxn.doTransaction();
  do_check_true(annotationService.itemHasAnnotation(postDataId, POST_DATA_ANNO))
  do_check_eq(annotationService.getItemAnnotation(postDataId, POST_DATA_ANNO), postData);
  postDataTxn.undoTransaction();
  do_check_false(annotationService.itemHasAnnotation(postDataId, POST_DATA_ANNO))

  // Test editing item date added
  var oldAdded = bmsvc.getItemDateAdded(bkmk1Id);
  var newAdded = Date.now();
  var eidaTxn = ptSvc.editItemDateAdded(bkmk1Id, newAdded);
  eidaTxn.doTransaction();
  do_check_eq(newAdded, bmsvc.getItemDateAdded(bkmk1Id));
  eidaTxn.undoTransaction();
  do_check_eq(oldAdded, bmsvc.getItemDateAdded(bkmk1Id));

  // Test editing item last modified 
  var oldModified = bmsvc.getItemLastModified(bkmk1Id);
  var newModified = Date.now();
  var eilmTxn = ptSvc.editItemLastModified(bkmk1Id, newModified);
  eilmTxn.doTransaction();
  do_check_eq(newModified, bmsvc.getItemLastModified(bkmk1Id));
  eilmTxn.undoTransaction();
  do_check_eq(oldModified, bmsvc.getItemLastModified(bkmk1Id));
}
