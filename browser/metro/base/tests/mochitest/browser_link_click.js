/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function test() {
  waitForExplicitFinish();
  runTests();
}

gTests.push({
  desc: "regular link click",
  run: function () {
    let tab = yield addTab(chromeRoot + "browser_link_click.html");
    let tabCount = Browser.tabs.length;

    EventUtils.sendMouseEvent({type: "click"}, "link", tab.browser.contentWindow);
    yield waitForCondition(() => tab.browser.currentURI.spec == "about:blank");
    is(Browser.tabs.length, tabCount, "link loaded in the same tab");
  }
});

gTests.push({
  desc: "middle-click opens link in background tab",
  run: function () {
    let tab = yield addTab(chromeRoot + "browser_link_click.html");

    let tabOpen = waitForEvent(window, "TabOpen");
    EventUtils.sendMouseEvent({type: "click", button: 1}, "link", tab.browser.contentWindow);
    let event = yield tabOpen;

    let newTab = Browser.getTabFromChrome(event.originalTarget);
    yield waitForEvent(newTab.browser, "pageshow");

    is(newTab.browser.currentURI.spec, "about:blank");
    ok(newTab != Browser.selectedTab, "new tab is in the background");

    Browser.closeTab(newTab, { forceClose: true });
  }
});

gTests.push({
  desc: "shift-middle-click opens link in background tab",
  run: function () {
    let tab = yield addTab(chromeRoot + "browser_link_click.html");

    let tabOpen = waitForEvent(window, "TabOpen");
    EventUtils.sendMouseEvent({type: "click", button: 1, shiftKey: true}, "link", tab.browser.contentWindow);
    let event = yield tabOpen;

    let newTab = Browser.getTabFromChrome(event.originalTarget);
    yield waitForEvent(newTab.browser, "pageshow");

    is(newTab.browser.currentURI.spec, "about:blank");
    ok(newTab == Browser.selectedTab, "new tab is in the foreground");

    Browser.closeTab(newTab, { forceClose: true });
  }
});

gTests.push({
  desc: "ctrl-click opens link in background tab",
  run: function () {
    let tab = yield addTab(chromeRoot + "browser_link_click.html");

    let tabOpen = waitForEvent(window, "TabOpen");
    EventUtils.sendMouseEvent({type: "click", ctrlKey: true}, "link", tab.browser.contentWindow);
    let event = yield tabOpen;

    let newTab = Browser.getTabFromChrome(event.originalTarget);
    yield waitForEvent(newTab.browser, "pageshow");

    is(newTab.browser.currentURI.spec, "about:blank");
    ok(newTab != Browser.selectedTab, "new tab is in the background");

    Browser.closeTab(newTab, { forceClose: true });
  }
});

gTests.push({
  desc: "shift-ctrl-click opens link in background tab",
  run: function () {
    let tab = yield addTab(chromeRoot + "browser_link_click.html");

    let tabOpen = waitForEvent(window, "TabOpen");
    EventUtils.sendMouseEvent({type: "click", ctrlKey: true, shiftKey: true}, "link", tab.browser.contentWindow);
    let event = yield tabOpen;

    let newTab = Browser.getTabFromChrome(event.originalTarget);
    yield waitForEvent(newTab.browser, "pageshow");

    is(newTab.browser.currentURI.spec, "about:blank");
    ok(newTab == Browser.selectedTab, "new tab is in the foreground");

    Browser.closeTab(newTab, { forceClose: true });
  }
});
