"use strict";

// Helper that watches events that may be triggered when tab browsers are
// swapped during the test.
//
// The primary purpose of this helper is to access tab browser properties
// during tab events, to verify that there are no undesired side effects, as a
// regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1695346
class TabEventTracker {
  constructor(tab) {
    this.tab = tab;

    tab.addEventListener("TabAttrModified", this);
    tab.addEventListener("TabShow", this);
    tab.addEventListener("TabHide", this);
  }

  handleEvent(event) {
    let description = `${this._expectations.description} at ${event.type}`;
    if (event.type === "TabAttrModified") {
      description += `, changed=${event.detail.changed}`;
    }

    const browser = this.tab.linkedBrowser;
    is(
      browser.currentURI.spec,
      this._expectations.tabUrl,
      `${description} - expected currentURI`
    );
    ok(browser._cachedCurrentURI, `${description} - currentURI was cached`);

    if (event.type === "TabAttrModified") {
      if (event.detail.changed.includes("muted")) {
        if (browser.audioMuted) {
          this._counts.muted++;
        } else {
          this._counts.unmuted++;
        }
      }
    } else if (event.type === "TabShow") {
      this._counts.shown++;
    } else if (event.type === "TabHide") {
      this._counts.hidden++;
    } else {
      ok(false, `Unexpected event: ${event.type}`);
    }
  }

  setExpectations(expectations) {
    this._expectations = expectations;

    this._counts = {
      muted: 0,
      unmuted: 0,
      shown: 0,
      hidden: 0,
    };
  }

  checkExpectations() {
    const { description, counters, tabUrl } = this._expectations;
    Assert.deepEqual(
      this._counts,
      counters,
      `${description} - events observed while swapping tab`
    );
    let browser = this.tab.linkedBrowser;
    is(browser.currentURI.spec, tabUrl, `${description} - tab's currentURI`);

    // Tabs without titles default to URLs without scheme, according to the
    // logic of tabbrowser.js's setTabTitle/_setTabLabel.
    // TODO bug 1695512: lazy tabs deviate from that expectation, so the title
    // is the full URL instead of the URL with the scheme stripped.
    let tabTitle = tabUrl;
    is(browser.contentTitle, tabTitle, `${description} - tab's contentTitle`);
  }
}

add_task(async function test_hidden_muted_lazy_tabs_and_swapping() {
  const params = { createLazyBrowser: true };
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const URL_HIDDEN = "http://example.com/hide";
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const URL_MUTED = "http://example.com/mute";
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const URL_NORMAL = "http://example.com/back";

  const lazyTab = BrowserTestUtils.addTab(gBrowser, "", params);
  const mutedTab = BrowserTestUtils.addTab(gBrowser, URL_MUTED, params);
  const hiddenTab = BrowserTestUtils.addTab(gBrowser, URL_HIDDEN, params);
  const normalTab = BrowserTestUtils.addTab(gBrowser, URL_NORMAL, params);

  mutedTab.toggleMuteAudio();
  gBrowser.hideTab(hiddenTab);

  is(lazyTab.linkedPanel, "", "lazyTab is lazy");
  is(hiddenTab.linkedPanel, "", "hiddenTab is lazy");
  is(mutedTab.linkedPanel, "", "mutedTab is lazy");
  is(normalTab.linkedPanel, "", "normalTab is lazy");

  ok(mutedTab.linkedBrowser.audioMuted, "mutedTab is muted");
  ok(hiddenTab.hidden, "hiddenTab is hidden");
  ok(!lazyTab.linkedBrowser.audioMuted, "lazyTab was not muted");
  ok(!lazyTab.hidden, "lazyTab was not hidden");

  const tabEventTracker = new TabEventTracker(lazyTab);

  tabEventTracker.setExpectations({
    description: "mutedTab replaced lazyTab (initial)",
    counters: {
      muted: 1,
      unmuted: 0,
      shown: 0,
      hidden: 0,
    },
    tabUrl: URL_MUTED,
  });
  gBrowser.swapBrowsersAndCloseOther(lazyTab, mutedTab);
  tabEventTracker.checkExpectations();
  is(lazyTab.linkedPanel, "", "muted lazyTab is still lazy");
  ok(lazyTab.linkedBrowser.audioMuted, "muted lazyTab is now muted");
  ok(!lazyTab.hidden, "muted lazyTab is not hidden");

  tabEventTracker.setExpectations({
    description: "hiddenTab replaced lazyTab/mutedTab",
    counters: {
      muted: 0,
      unmuted: 1,
      shown: 0,
      hidden: 1,
    },
    tabUrl: URL_HIDDEN,
  });
  gBrowser.swapBrowsersAndCloseOther(lazyTab, hiddenTab);
  tabEventTracker.checkExpectations();
  is(lazyTab.linkedPanel, "", "hidden lazyTab is still lazy");
  ok(!lazyTab.linkedBrowser.audioMuted, "hidden lazyTab is not muted any more");
  ok(lazyTab.hidden, "hidden lazyTab has been hidden");

  tabEventTracker.setExpectations({
    description: "normalTab replaced lazyTab/hiddenTab",
    counters: {
      muted: 0,
      unmuted: 0,
      shown: 1,
      hidden: 0,
    },
    tabUrl: URL_NORMAL,
  });
  gBrowser.swapBrowsersAndCloseOther(lazyTab, normalTab);
  tabEventTracker.checkExpectations();
  is(lazyTab.linkedPanel, "", "normal lazyTab is still lazy");
  ok(!lazyTab.linkedBrowser.audioMuted, "normal lazyTab is not muted any more");
  ok(!lazyTab.hidden, "normal lazyTab is not hidden any more");

  BrowserTestUtils.removeTab(lazyTab);
});
