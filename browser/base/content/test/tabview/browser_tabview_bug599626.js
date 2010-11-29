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
 * The Original Code is tabview bug 599626 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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

let handleDialog;

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let groupItemOne = contentWindow.GroupItems.getActiveGroupItem();

  // Create a group and make it active
  let box = new contentWindow.Rect(10, 10, 300, 300);
  let groupItemTwo = new contentWindow.GroupItem([], { bounds: box });
  contentWindow.GroupItems.setActiveGroupItem(groupItemTwo);

  let testTab = 
    gBrowser.addTab(
      "http://mochi.test:8888/browser/browser/base/content/test/tabview/test_bug599626.html");
  let browser = gBrowser.getBrowserForTab(testTab);
  let onLoad = function() {
    browser.removeEventListener("load", onLoad, true);

    testStayOnPage(contentWindow, groupItemOne, groupItemTwo);
  }
  browser.addEventListener("load", onLoad, true);
}

function testStayOnPage(contentWindow, groupItemOne, groupItemTwo) {
  setupAndRun(contentWindow, groupItemOne, groupItemTwo, function(doc) {
    groupItemTwo.addSubscriber(groupItemTwo, "groupShown", function() {
      groupItemTwo.removeSubscriber(groupItemTwo, "groupShown");

      is(gBrowser.tabs.length, 2, 
         "The total number of tab is 2 when staying on the page");
      is(contentWindow.TabItems.getItems().length, 2, 
         "The total number of tab items is 2 when staying on the page");

      let onTabViewShown = function() {
        window.removeEventListener("tabviewshown", onTabViewShown, false);

        // start the next test
        testLeavePage(contentWindow, groupItemOne, groupItemTwo);
      };
      window.addEventListener("tabviewshown", onTabViewShown, false);
      TabView.toggle();
    });
    // stay on page
    doc.documentElement.getButton("cancel").click();
  });
}

function testLeavePage(contentWindow, groupItemOne, groupItemTwo) {
  setupAndRun(contentWindow, groupItemOne, groupItemTwo, function(doc) {
    // clean up and finish the test
    groupItemTwo.addSubscriber(groupItemTwo, "close", function() {
      groupItemTwo.removeSubscriber(groupItemTwo, "close");

      is(gBrowser.tabs.length, 1,
         "The total number of tab is 1 after leaving the page");
      is(contentWindow.TabItems.getItems().length, 1, 
         "The total number of tab items is 1 after leaving the page");

      let endGame = function() {
        window.removeEventListener("tabviewhidden", endGame, false);
        finish();
      };
      window.addEventListener("tabviewhidden", endGame, false);
    });

    // Leave page
    doc.documentElement.getButton("accept").click();
  });
}

function setupAndRun(contentWindow, groupItemOne, groupItemTwo, callback) {
  let closeButton = groupItemTwo.container.getElementsByClassName("close");
  ok(closeButton[0], "Group close button exists");
  // click the close button
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    handleDialog = function(doc) {
      callback(doc);
    };
    startCallbackTimer();
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  let tabItem = groupItemOne.getChild(0);
  tabItem.zoomIn();
}

// Copied from http://mxr.mozilla.org/mozilla-central/source/toolkit/components/places/tests/mochitest/prompt_common.js
let observer = {
  QueryInterface : function (iid) {
    const interfaces = [Ci.nsIObserver, Ci.nsISupports, Ci.nsISupportsWeakReference];

    if (!interfaces.some( function(v) { return iid.equals(v) } ))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  observe : function (subject, topic, data) {
    let doc = getDialogDoc();
    if (doc)
      handleDialog(doc);
    else
      startCallbackTimer(); // try again in a bit
  }
};

function startCallbackTimer() {
   // Delay before the callback twiddles the prompt.
   const dialogDelay = 10;

   // Use a timer to invoke a callback to twiddle the authentication dialog
   let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
   timer.init(observer, dialogDelay, Ci.nsITimer.TYPE_ONE_SHOT);
}

function getDialogDoc() {
  // Find the <browser> which contains notifyWindow, by looking
  // through all the open windows and all the <browsers> in each.
  let wm = Cc["@mozilla.org/appshell/window-mediator;1"].
            getService(Ci.nsIWindowMediator);
  let enumerator = wm.getXULWindowEnumerator(null);

   while (enumerator.hasMoreElements()) {
     let win = enumerator.getNext();
     let windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;
 
     let containedDocShells = windowDocShell.getDocShellEnumerator(
                                Ci.nsIDocShellTreeItem.typeChrome,
                                Ci.nsIDocShell.ENUMERATE_FORWARDS);
     while (containedDocShells.hasMoreElements()) {
       // Get the corresponding document for this docshell
       let childDocShell = containedDocShells.getNext();
       // We don't want it if it's not done loading.
       if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
         continue;
       let childDoc = childDocShell.QueryInterface(Ci.nsIDocShell).
                      contentViewer.DOMDocument;

       if (childDoc.location.href == "chrome://global/content/commonDialog.xul")
         return childDoc;
     }
   }
 
  return null;
}
