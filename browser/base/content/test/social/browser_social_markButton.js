/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let prefName = "social.enabled",
    gFinishCB;

function test() {
  waitForExplicitFinish();

  // Need to load a http/https/ftp/ftps page for the social mark button to appear
  let tab = gBrowser.selectedTab = gBrowser.addTab("https://example.com", {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    executeSoon(tabLoaded);
  }, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(prefName);
    gBrowser.removeTab(tab);
  });
}

function tabLoaded() {
  ok(Social, "Social module loaded");

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    gFinishCB = finishcb;
    testInitial();
  });
}

function testInitial(finishcb) {
  ok(Social.provider, "Social provider is active");
  ok(Social.provider.enabled, "Social provider is enabled");
  let port = Social.provider.getWorkerPort();
  ok(port, "Social provider has a port to its FrameWorker");
  port.close();

  let markButton = SocialMark.button;
  ok(markButton, "mark button exists");

  // ensure the worker initialization and handshakes are all done and we
  // have a profile and the worker has sent a page-mark-config msg.
  waitForCondition(function() Social.provider.pageMarkInfo != null, function() {
    is(markButton.hasAttribute("marked"), false, "SocialMark button should not have 'marked' attribute before mark button is clicked");
    // Check the strings from our worker actually ended up on the button.
    is(markButton.getAttribute("tooltiptext"), "Mark this page", "check tooltip text is correct");
    // Check the relative URL was resolved correctly (note this image has offsets of zero...)
    is(markButton.style.listStyleImage, 'url("https://example.com/browser/browser/base/content/test/social/social_mark_image.png")', "check image url is correct");

    // Test the mark button command handler
    SocialMark.togglePageMark(function() {
      is(markButton.hasAttribute("marked"), true, "mark button should have 'marked' attribute after mark button is clicked");
      is(markButton.getAttribute("tooltiptext"), "Unmark this page", "check tooltip text is correct");
      // Check the URL and offsets were applied correctly
      is(markButton.style.listStyleImage, 'url("https://example.com/browser/browser/base/content/test/social/social_mark_image.png")', "check image url is correct");
      SocialMark.togglePageMark(function() {
        is(markButton.hasAttribute("marked"), false, "mark button should not be marked");
        executeSoon(function() {
          testStillMarkedIn2Tabs();
        });
      });
    });
    markButton.click();
  }, "provider didn't provide page-mark-config");
}

function testStillMarkedIn2Tabs() {
  let toMark = "http://example.com";
  let markUri = Services.io.newURI(toMark, null, null);
  let markButton = SocialMark.button;
  let initialTab = gBrowser.selectedTab;
  is(markButton.hasAttribute("marked"), false, "SocialMark button should not have 'marked' for the initial tab");
  let tab1 = gBrowser.selectedTab = gBrowser.addTab(toMark);
  let tab1b = gBrowser.getBrowserForTab(tab1);

  tab1b.addEventListener("load", function tabLoad(event) {
    tab1b.removeEventListener("load", tabLoad, true);
    let tab2 = gBrowser.selectedTab = gBrowser.addTab(toMark);
    let tab2b = gBrowser.getBrowserForTab(tab2);
    tab2b.addEventListener("load", function tabLoad(event) {
      tab2b.removeEventListener("load", tabLoad, true);
      // should start without either page being marked.
      is(markButton.hasAttribute("marked"), false, "SocialMark button should not have 'marked' before we've done anything");
      Social.isURIMarked(markUri, function(marked) {
        ok(!marked, "page is unmarked in annotations");
        markButton.click();
        waitForCondition(function() markButton.hasAttribute("marked"), function() {
          Social.isURIMarked(markUri, function(marked) {
            ok(marked, "page is marked in annotations");
            // and switching to the first tab (with the same URL) should still reflect marked.
            gBrowser.selectedTab = tab1;
            is(markButton.hasAttribute("marked"), true, "SocialMark button should reflect the marked state");
            // but switching back the initial one should reflect not marked.
            gBrowser.selectedTab = initialTab;
            waitForCondition(function() !markButton.hasAttribute("marked"), function() {
              gBrowser.selectedTab = tab1;
    
              SocialMark.togglePageMark(function() {
                Social.isURIMarked(gBrowser.currentURI, function(marked) {
                  ok(!marked, "page is unmarked in annotations");
                  is(markButton.hasAttribute("marked"), false, "mark button should not be marked");
                  gBrowser.removeTab(tab1);
                  gBrowser.removeTab(tab2);
                  executeSoon(testStillMarkedAfterReopen);
                });
              });
            }, "button has been unmarked");
          });
        }, "button has been marked");
      });

    }, true);
  }, true);
}

function testStillMarkedAfterReopen() {
  let toMark = "http://example.com";
  let markButton = SocialMark.button;

  is(markButton.hasAttribute("marked"), false, "Reopen: SocialMark button should not have 'marked' for the initial tab");
  let tab = gBrowser.selectedTab = gBrowser.addTab(toMark);
  let tabb = gBrowser.getBrowserForTab(tab);
  tabb.addEventListener("load", function tabLoad(event) {
    tabb.removeEventListener("load", tabLoad, true);
    SocialMark.togglePageMark(function() {
      is(markButton.hasAttribute("marked"), true, "SocialMark button should reflect the marked state");
      gBrowser.removeTab(tab);
      // should be on the initial unmarked tab now.
      waitForCondition(function() !markButton.hasAttribute("marked"), function() {
        // now open the same URL - should be back to Marked.
        tab = gBrowser.selectedTab = gBrowser.addTab(toMark, {skipAnimation: true});
        tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
          tab.linkedBrowser.removeEventListener("load", tabLoad, true);
          executeSoon(function() {
            is(markButton.hasAttribute("marked"), true, "New tab to previously marked URL should reflect marked state");
            SocialMark.togglePageMark(function() {
              gBrowser.removeTab(tab);
              executeSoon(testOnlyMarkCertainUrlsTabSwitch);
            });
          });
        }, true);
      }, "button is now unmarked");
    });
  }, true);
}

function testOnlyMarkCertainUrlsTabSwitch() {
  let toMark = "http://example.com";
  let notSharable = "about:blank";
  let markButton = SocialMark.button;
  let tab = gBrowser.selectedTab = gBrowser.addTab(toMark);
  let tabb = gBrowser.getBrowserForTab(tab);
  tabb.addEventListener("load", function tabLoad(event) {
    tabb.removeEventListener("load", tabLoad, true);
    ok(!markButton.hidden, "SocialMark button not hidden for http url");
    let tab2 = gBrowser.selectedTab = gBrowser.addTab(notSharable);
    let tabb2 = gBrowser.getBrowserForTab(tab2);
    tabb2.addEventListener("load", function tabLoad(event) {
      tabb2.removeEventListener("load", tabLoad, true);
      ok(markButton.disabled, "SocialMark button disabled for about:blank");
      gBrowser.selectedTab = tab;
      ok(!markButton.disabled, "SocialMark button re-shown when switching back to http: url");
      gBrowser.selectedTab = tab2;
      ok(markButton.disabled, "SocialMark button re-hidden when switching back to about:blank");
      gBrowser.removeTab(tab);
      gBrowser.removeTab(tab2);
      executeSoon(testOnlyMarkCertainUrlsSameTab);
    }, true);
  }, true);
}

function testOnlyMarkCertainUrlsSameTab() {
  let toMark = "http://example.com";
  let notSharable = "about:blank";
  let markButton = SocialMark.button;
  let tab = gBrowser.selectedTab = gBrowser.addTab(toMark);
  let tabb = gBrowser.getBrowserForTab(tab);
  tabb.addEventListener("load", function tabLoad(event) {
    tabb.removeEventListener("load", tabLoad, true);
    ok(!markButton.disabled, "SocialMark button not disabled for http url");
    tabb.addEventListener("load", function tabLoad(event) {
      tabb.removeEventListener("load", tabLoad, true);
      ok(markButton.disabled, "SocialMark button disabled for about:blank");
      tabb.addEventListener("load", function tabLoad(event) {
        tabb.removeEventListener("load", tabLoad, true);
        ok(!markButton.disabled, "SocialMark button re-enabled http url");
        gBrowser.removeTab(tab);
        executeSoon(testDisable);
      }, true);
      tabb.loadURI(toMark);
    }, true);
    tabb.loadURI(notSharable);
  }, true);
}

function testDisable() {
  let markButton = SocialMark.button;
  Services.prefs.setBoolPref(prefName, false);
  is(markButton.hidden, true, "SocialMark button should be hidden when pref is disabled");
  gFinishCB();
}
