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
 * The Original Code is tabview private browsing test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 * Ian Gilman <ian@iangilman.com>
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

let contentWindow = null;
let normalURLs = []; 
let pbTabURL = "about:privatebrowsing";
let groupTitles = [];
let normalIteration = 0;

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

// -----------
function test() {
  waitForExplicitFinish();

  // Go into Tab View
  window.addEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  TabView.toggle();
}

// -----------
function onTabViewLoadedAndShown() {
  window.removeEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  ok(TabView.isVisible(), "Tab View is visible");

  // Establish initial state
  contentWindow = document.getElementById("tab-view").contentWindow;  
  verifyCleanState("start");
  
  // register a clean up for private browsing just in case
  registerCleanupFunction(function() {
    pb.privateBrowsingEnabled = false;
  });
  
  // create a group
  let box = new contentWindow.Rect(20, 20, 180, 180);
  let groupItem = new contentWindow.GroupItem([], {bounds: box, title: "test1"});
  let id = groupItem.id; 
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  registerCleanupFunction(function() {
    contentWindow.GroupItems.groupItem(id).close();
  });
  
  // make it the active group so new tabs will be added to it
  contentWindow.GroupItems.setActiveGroupItem(groupItem);
  
  // collect the group titles
  let count = contentWindow.GroupItems.groupItems.length;
  for (let a = 0; a < count; a++) {
    let gi = contentWindow.GroupItems.groupItems[a];
    groupTitles[a] = gi.getTitle();
  }
  
  // Create a second tab
  gBrowser.addTab("about:robots");
  is(gBrowser.tabs.length, 2, "we now have 2 tabs");
  registerCleanupFunction(function() {
    gBrowser.removeTab(gBrowser.tabs[1]);
  });

  afterAllTabsLoaded(function() {
    // Get normal tab urls
    for (let a = 0; a < gBrowser.tabs.length; a++)
      normalURLs.push(gBrowser.tabs[a].linkedBrowser.currentURI.spec);

    // verify that we're all set up for our test
    verifyNormal();

    // go into private browsing and make sure Tab View becomes hidden
    togglePBAndThen(function() {
      ok(!TabView.isVisible(), "Tab View is no longer visible");
      verifyPB();
      
      // exit private browsing and make sure Tab View is shown again
      togglePBAndThen(function() {
        ok(TabView.isVisible(), "Tab View is visible again");
        verifyNormal();
        
        // exit Tab View
        window.addEventListener("tabviewhidden", onTabViewHidden, false);
        TabView.toggle();
      });  
    });
  });
}

// -----------
function onTabViewHidden() {
  window.removeEventListener("tabviewhidden", onTabViewHidden, false);
  ok(!TabView.isVisible(), "Tab View is not visible");
  
  // go into private browsing and make sure Tab View remains hidden
  togglePBAndThen(function() {
    ok(!TabView.isVisible(), "Tab View is still not visible");
    verifyPB();
    
    // turn private browsing back off
    togglePBAndThen(function() {
      verifyNormal();
      
      // end game
      ok(!TabView.isVisible(), "we finish with Tab View not visible");
      registerCleanupFunction(verifyCleanState); // verify after all cleanups
      finish();
    });
  });
}

// ----------
function verifyCleanState(mode) {
  let prefix = "we " + (mode || "finish") + " with ";
  is(gBrowser.tabs.length, 1, prefix + "one tab");
  is(contentWindow.GroupItems.groupItems.length, 1, prefix + "1 group");
  ok(gBrowser.tabs[0]._tabViewTabItem.parent == contentWindow.GroupItems.groupItems[0], 
      "the tab is in the group");
  ok(!pb.privateBrowsingEnabled, prefix + "private browsing off");
}

// ----------
function verifyPB() {
  ok(pb.privateBrowsingEnabled == true, "private browsing is on");
  is(gBrowser.tabs.length, 1, "we have 1 tab in private browsing");
  is(contentWindow.GroupItems.groupItems.length, 1, "we have 1 group in private browsing");
  ok(gBrowser.tabs[0]._tabViewTabItem.parent == contentWindow.GroupItems.groupItems[0], 
      "the tab is in the group");

  let browser = gBrowser.tabs[0].linkedBrowser;
  is(browser.currentURI.spec, pbTabURL, "correct URL for private browsing");
}

// ----------
function verifyNormal() {
  let prefix = "verify normal " + normalIteration + ": "; 
  normalIteration++;
  
  ok(pb.privateBrowsingEnabled == false, prefix + "private browsing is off");
  
  let tabCount = gBrowser.tabs.length;
  let groupCount = contentWindow.GroupItems.groupItems.length;
  is(tabCount, 2, prefix + "we have 2 tabs");
  is(groupCount, 2, prefix + "we have 2 groups");
  ok(tabCount == groupCount, prefix + "same number of tabs as groups"); 
  for (let a = 0; a < tabCount; a++) {
    let tab = gBrowser.tabs[a];
    is(tab.linkedBrowser.currentURI.spec, normalURLs[a],
        prefix + "correct URL");

    let groupItem = contentWindow.GroupItems.groupItems[a];
    is(groupItem.getTitle(), groupTitles[a], prefix + "correct group title");
    
    ok(tab._tabViewTabItem.parent == groupItem,
        prefix + "tab " + a + " is in group " + a);
  }
}

// ----------
function togglePBAndThen(callback) {
  function pbObserver(aSubject, aTopic, aData) {
    if (aTopic != "private-browsing-transition-complete")
      return;

    Services.obs.removeObserver(pbObserver, "private-browsing-transition-complete");
    
    afterAllTabsLoaded(callback);
  }

  Services.obs.addObserver(pbObserver, "private-browsing-transition-complete", false);
  pb.privateBrowsingEnabled = !pb.privateBrowsingEnabled;
}
