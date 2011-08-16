/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

  showTabView(onTabViewLoadedAndShown);
}

// -----------
function onTabViewLoadedAndShown() {
  ok(TabView.isVisible(), "Tab View is visible");

  // Establish initial state
  contentWindow = TabView.getContentWindow();
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
  contentWindow.UI.setActive(groupItem);

  // collect the group titles
  let count = contentWindow.GroupItems.groupItems.length;
  for (let a = 0; a < count; a++) {
    let gi = contentWindow.GroupItems.groupItems[a];
    groupTitles[a] = gi.getTitle();
  }

  contentWindow.gPrefBranch.setBoolPref("animate_zoom", false);

  // Create a second tab
  gBrowser.addTab("about:robots");
  is(gBrowser.tabs.length, 2, "we now have 2 tabs");

  registerCleanupFunction(function() {
    gBrowser.removeTab(gBrowser.tabs[1]);
    contentWindow.gPrefBranch.clearUserPref("animate_zoom");
  });

  afterAllTabsLoaded(function() {
    // Get normal tab urls
    for (let a = 0; a < gBrowser.tabs.length; a++)
      normalURLs.push(gBrowser.tabs[a].linkedBrowser.currentURI.spec);

    // verify that we're all set up for our test
    verifyNormal();

    // go into private browsing and make sure Tab View becomes hidden
    togglePrivateBrowsing(function() {
      whenTabViewIsHidden(function() {
        ok(!TabView.isVisible(), "Tab View is no longer visible");
        verifyPB();

        // exit private browsing and make sure Tab View is shown again
        togglePrivateBrowsing(function() {
          whenTabViewIsShown(function() {
            ok(TabView.isVisible(), "Tab View is visible again");
            verifyNormal();

            hideTabView(onTabViewHidden);
          });
        });
      });
    });
  });
}

// -----------
function onTabViewHidden() {
  ok(!TabView.isVisible(), "Tab View is not visible");
  
  // go into private browsing and make sure Tab View remains hidden
  togglePrivateBrowsing(function() {
    ok(!TabView.isVisible(), "Tab View is still not visible");
    verifyPB();
    
    // turn private browsing back off
    togglePrivateBrowsing(function() {
      verifyNormal();
      
      // end game
      ok(!TabView.isVisible(), "we finish with Tab View not visible");
      registerCleanupFunction(verifyCleanState); // verify after all cleanups

      gBrowser.selectedTab = gBrowser.tabs[0];
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
