//* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared Places Import - change other consumers if you change this: */
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});
XPCOMUtils.defineLazyScriptGetter(
  this,
  "PlacesTreeView",
  "chrome://browser/content/places/treeView.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["PlacesInsertionPoint", "PlacesController", "PlacesControllerDragHelper"],
  "chrome://browser/content/places/controller.js"
);
/* End Shared Places Import */

/**
 * SelectBookmarkDialog controls the user interface for the "Use Bookmark for
 * Home Page" dialog.
 *
 * The caller (gMainPane.setHomePageToBookmark in main.js) invokes this dialog
 * with a single argument - a reference to an object with a .urls property and
 * a .names property.  This dialog is responsible for updating the contents of
 * the .urls property with an array of URLs to use as home pages and for
 * updating the .names property with an array of names for those URLs before it
 * closes.
 */
var SelectBookmarkDialog = {
  init: function SBD_init() {
    document.getElementById("bookmarks").place =
      "place:type=" + Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY;

    // Initial update of the OK button.
    this.selectionChanged();
    document.addEventListener("dialogaccept", function() {
      SelectBookmarkDialog.accept();
    });
  },

  /**
   * Update the disabled state of the OK button as the user changes the
   * selection within the view.
   */
  selectionChanged: function SBD_selectionChanged() {
    var accept = document
      .getElementById("selectBookmarkDialog")
      .getButton("accept");
    var bookmarks = document.getElementById("bookmarks");
    var disableAcceptButton = true;
    if (bookmarks.hasSelection) {
      if (!PlacesUtils.nodeIsSeparator(bookmarks.selectedNode)) {
        disableAcceptButton = false;
      }
    }
    accept.disabled = disableAcceptButton;
  },

  onItemDblClick: function SBD_onItemDblClick() {
    var bookmarks = document.getElementById("bookmarks");
    var selectedNode = bookmarks.selectedNode;
    if (selectedNode && PlacesUtils.nodeIsURI(selectedNode)) {
      /**
       * The user has double clicked on a tree row that is a link. Take this to
       * mean that they want that link to be their homepage, and close the dialog.
       */
      document
        .getElementById("selectBookmarkDialog")
        .getButton("accept")
        .click();
    }
  },

  /**
   * User accepts their selection. Set all the selected URLs or the contents
   * of the selected folder as the list of homepages.
   */
  accept: function SBD_accept() {
    var bookmarks = document.getElementById("bookmarks");
    if (!bookmarks.hasSelection) {
      throw new Error(
        "Should not be able to accept dialog if there is no selected URL!"
      );
    }
    var urls = [];
    var names = [];
    var selectedNode = bookmarks.selectedNode;
    if (PlacesUtils.nodeIsFolder(selectedNode)) {
      let concreteGuid = PlacesUtils.getConcreteItemGuid(selectedNode);
      var contents = PlacesUtils.getFolderContents(concreteGuid).root;
      var cc = contents.childCount;
      for (var i = 0; i < cc; ++i) {
        var node = contents.getChild(i);
        if (PlacesUtils.nodeIsURI(node)) {
          urls.push(node.uri);
          names.push(node.title);
        }
      }
      contents.containerOpen = false;
    } else {
      urls.push(selectedNode.uri);
      names.push(selectedNode.title);
    }
    window.arguments[0].urls = urls;
    window.arguments[0].names = names;
  },
};
