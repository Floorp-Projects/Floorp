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
 * The Original Code is Places test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>
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

// Get history services
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var gh = hs.QueryInterface(Ci.nsIGlobalHistory2);
var bh = hs.QueryInterface(Ci.nsIBrowserHistory);
var ts = Cc["@mozilla.org/browser/tagging-service;1"].
         getService(Components.interfaces.nsITaggingService);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

function add_visit(aURI, aReferrer) {
  var visitId = hs.addVisit(aURI,
                            Date.now() * 1000,
                            aReferrer,
                            hs.TRANSITION_TYPED, // user typed in URL bar
                            false, // not redirect
                            0);
  return visitId;
}

function add_bookmark(aURI) {
  var bId = bs.insertBookmark(bs.unfiledBookmarksFolder, aURI,
                              bs.DEFAULT_INDEX, "bookmark/" + aURI.spec);
  return bId;
}

const TEST_URL = "http://example.com/";
const MOZURISPEC = "http://mozilla.com/";

function test() {
  waitForExplicitFinish();
  var win = window.openDialog("chrome://browser/content/places/places.xul",
                              "",
                              "chrome,toolbar=yes,dialog=no,resizable");

  win.addEventListener("load", function onload() {
    win.removeEventListener("load", onload, false);
    executeSoon(function () {
      var PU = win.PlacesUtils;
      var PO = win.PlacesOrganizer;
      var PUIU = win.PlacesUIUtils;

      // individual tests for each step of tagging a history item
      var tests = {

        sanity: function(){
          // sanity check
          ok(PU, "PlacesUtils in scope");
          ok(PUIU, "PlacesUIUtils in scope");
          ok(PO, "Places organizer in scope");
        },

        makeHistVisit: function() {
          // need to add a history object
          var testURI1 = PU._uri(MOZURISPEC);
          isnot(testURI1, null, "testURI is not null");
          var visitId = add_visit(testURI1);
          ok(visitId > 0, "A visit was added to the history");
          ok(gh.isVisited(testURI1), MOZURISPEC + " is a visited url.");
        },

        makeTag: function() {
          // create an initial tag to work with
          var bmId = add_bookmark(PlacesUtils._uri(TEST_URL));
          ok(bmId > 0, "A bookmark was added");
          ts.tagURI(PlacesUtils._uri(TEST_URL), ["foo"]);
          var tags = ts.getTagsForURI(PU._uri(TEST_URL), {});
          is(tags[0], 'foo', "tag is foo");
        },

        focusTag: function (paste){
          // focus the new tag
          PO.selectLeftPaneQuery("Tags");
          var tags = PO._places.selectedNode;
          tags.containerOpen = true;
          var fooTag = tags.getChild(0);
          this.tagNode = fooTag;
          PO._places.selectNode(fooTag);
          is(this.tagNode.title, 'foo', "tagNode title is foo");
          var ip = PO._places.insertionPoint;
          ok(ip.isTag, "IP is a tag");
          if (paste) {
            ok(true, "About to paste");
            PO._places.controller.paste();
          }
        },

        histNode: null,

        copyHistNode: function (){
          // focus the history object
          PO.selectLeftPaneQuery("History");
          var histContainer = PO._places.selectedNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
          histContainer.containerOpen = true;
          PO._places.selectNode(histContainer.getChild(0));
          this.histNode = PO._content.view.nodeForTreeIndex(0);
          PO._content.selectNode(this.histNode);
          is(this.histNode.uri, MOZURISPEC,
             "historyNode exists: " + this.histNode.uri);
          // copy the history node
          PO._content.controller.copy();
        },

        waitForPaste: function (){
          try {
            var xferable = Cc["@mozilla.org/widget/transferable;1"].
                           createInstance(Ci.nsITransferable);
            xferable.addDataFlavor(PU.TYPE_X_MOZ_PLACE);
            PUIU.clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);
            var data = { }, type = { };
            xferable.getAnyTransferData(type, data, { });
            // Data is in the clipboard
            continue_test();
          } catch (ex) {
            // check again after 100ms.
            setTimeout(tests.waitForPaste, 100);
          }
        },

        pasteToTag: function (){
          // paste history node into tag
          this.focusTag(true);
        },

        historyNode: function (){
          // re-focus the history again
          PO.selectLeftPaneQuery("History");
          var histContainer = PO._places.selectedNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
          histContainer.containerOpen = true;
          PO._places.selectNode(histContainer.getChild(0));
          var histNode = PO._content.view.nodeForTreeIndex(0);
          ok(histNode, "histNode exists: " + histNode.title);
          // check to see if the history node is tagged!
          var tags = PU.tagging.getTagsForURI(PU._uri(MOZURISPEC), {});
          ok(tags.length == 1, "history node is tagged: " + tags.length);
          // check if a bookmark was created
          var isBookmarked = PU.bookmarks.isBookmarked(PU._uri(MOZURISPEC));
          is(isBookmarked, true, MOZURISPEC + " is bookmarked");
          var bookmarkIds = PU.bookmarks.getBookmarkIdsForURI(
                              PU._uri(histNode.uri), {});
          ok(bookmarkIds.length > 0, "bookmark exists for the tagged history item: " + bookmarkIds);
        },

        checkForBookmarkInUI: function(){
          // is the bookmark visible in the UI?
          // get the Unsorted Bookmarks node
          PO.selectLeftPaneQuery("UnfiledBookmarks");
          // now we can see what is in the _content tree
          var unsortedNode = PO._content.view.nodeForTreeIndex(1);
          ok(unsortedNode, "unsortedNode is not null: " + unsortedNode.uri);
          is(unsortedNode.uri, MOZURISPEC, "node uri's are the same");
        },

        tagNode: null,

        cleanUp: function(){
          ts.untagURI(PU._uri(MOZURISPEC), ["foo"]);
          ts.untagURI(PU._uri(TEST_URL), ["foo"]);
          hs.removeAllPages();
          var tags = ts.getTagsForURI(PU._uri(TEST_URL), {});
          is(tags.length, 0, "tags are gone");
          bs.removeFolderChildren(bs.unfiledBookmarksFolder);
        }
      };

      tests.sanity();
      tests.makeHistVisit();
      tests.makeTag();
      tests.focusTag();
      tests.copyHistNode();
      tests.waitForPaste();
      
      function continue_test() {
        tests.pasteToTag();
        tests.historyNode();
        tests.checkForBookmarkInUI();

        // remove new places data we created
        tests.cleanUp();

        win.close();
        finish();
      }

    });
  },false);
}
