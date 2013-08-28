/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  runTests();
}

function doEdgeUIGesture() {
  let event = document.createEvent("Events");
  event.initEvent("MozEdgeUICompleted", true, false);
  window.dispatchEvent(event);
}

function getpage(idx) {
  return "http://mochi.test:8888/metro/browser/metro/base/tests/mochitest/" + "res/blankpage" + idx + ".html";
}

gTests.push({
  desc: "Context UI on about:start",
  run: function testAboutStart() {
    let tab = yield addTab("about:start");

    yield waitForCondition(function () {
      return BrowserUI.isStartTabVisible;
      });

    is(BrowserUI.isStartTabVisible, true, "Start UI is displayed on about:start");
    is(ContextUI.navbarVisible, true, "Navbar is displayed on about:start");
    is(ContextUI.tabbarVisible, false, "Tabbar is not displayed initially");
    is(ContextUI.contextAppbarVisible, false, "Appbar is not displayed initially");

    // toggle on
    doEdgeUIGesture();
    is(ContextUI.navbarVisible, true, "Navbar is still visible after one swipe");
    is(ContextUI.tabbarVisible, true, "Tabbar is visible after one swipe");
    is(ContextUI.contextAppbarVisible, false, "Appbar is hidden after one swipe");

    // toggle off
    doEdgeUIGesture();
    is(ContextUI.navbarVisible, true, "Navbar is still visible after second swipe");
    is(ContextUI.tabbarVisible, false, "Tabbar is hidden after second swipe");
    is(ContextUI.contextAppbarVisible, false, "Appbar is hidden after second swipe");

    // sanity check - toggle on again
    doEdgeUIGesture();
    is(ContextUI.navbarVisible, true, "Navbar is still visible after third swipe");
    is(ContextUI.tabbarVisible, true, "Tabbar is visible after third swipe");
    is(ContextUI.contextAppbarVisible, false, "Appbar is hidden after third swipe");

    is(BrowserUI.isStartTabVisible, true, "Start UI is still visible");

    Browser.closeTab(tab, { forceClose: true });
  }
});

gTests.push({
  desc: "Context UI on a web page (about:)",
  run: function testAbout() {
    let tab = yield addTab("about:");
    ContextUI.dismiss();
    is(BrowserUI.isStartTabVisible, false, "Start UI is not visible on about:");
    is(ContextUI.navbarVisible, false, "Navbar is not initially visible on about:");
    is(ContextUI.tabbarVisible, false, "Tabbar is not initially visible on about:");

    doEdgeUIGesture();
    is(ContextUI.navbarVisible, true, "Navbar is visible after one swipe");
    is(ContextUI.tabbarVisible, true, "Tabbar is visble after one swipe");

    doEdgeUIGesture();
    is(ContextUI.navbarVisible, false, "Navbar is not visible after second swipe");
    is(ContextUI.tabbarVisible, false, "Tabbar is not visible after second swipe");

    is(BrowserUI.isStartTabVisible, false, "Start UI is still not visible");

    Browser.closeTab(tab, { forceClose: true });
  }
});

gTests.push({
  desc: "Control-L keyboard shortcut",
  run: function testAbout() {
    let tab = yield addTab("about:");
    ContextUI.dismiss();
    is(ContextUI.navbarVisible, false, "Navbar is not initially visible");
    is(ContextUI.tabbarVisible, false, "Tab bar is not initially visible");

    EventUtils.synthesizeKey('l', { accelKey: true });
    is(ContextUI.navbarVisible, true, "Navbar is visible");
    is(ContextUI.tabbarVisible, false, "Tab bar is not visible");

    let edit = document.getElementById("urlbar-edit");
    is(edit.value, "about:", "Location field contains the page URL");
    ok(document.commandDispatcher.focusedElement, edit.inputField, "Location field is focused");
    is(edit.selectionStart, 0, "Location field is selected");
    is(edit.selectionEnd, edit.value.length, "Location field is selected");

    edit.selectionEnd = 0;
    is(edit.selectionStart, 0, "Location field is unselected");
    is(edit.selectionEnd, 0, "Location field is unselected");

    EventUtils.synthesizeKey('l', { accelKey: true });
    is(edit.selectionStart, 0, "Location field is selected again");
    is(edit.selectionEnd, edit.value.length, "Location field is selected again");

    Browser.closeTab(tab, { forceClose: true });
  }
});

gTests.push({
  desc: "taps vs context ui dismissal",
  run: function () {
    // off by default
    InputSourceHelper.isPrecise = false;
    InputSourceHelper.fireUpdate();

    let tab = yield addTab("about:mozilla");

    ok(ContextUI.navbarVisible, "navbar visible after open");

    let navButtonDisplayPromise = waitForEvent(NavButtonSlider.back, "transitionend");

    yield loadUriInActiveTab(getpage(1));

    is(tab.browser.currentURI.spec, getpage(1), getpage(1));
    ok(ContextUI.navbarVisible, "navbar visible after navigate 1");

    yield loadUriInActiveTab(getpage(2));

    is(tab.browser.currentURI.spec, getpage(2), getpage(2));
    ok(ContextUI.navbarVisible, "navbar visible after navigate 2");

    yield loadUriInActiveTab(getpage(3));

    is(tab.browser.currentURI.spec, getpage(3), getpage(3));
    ok(ContextUI.navbarVisible, "navbar visible after navigate 3");

    // These transition in after we navigate. If we click on one of
    // them before they are visible they don't work, so wait for
    // display to occur.
    yield navButtonDisplayPromise;

    yield navBackViaNavButton();
    yield waitForCondition2(function () { return tab.browser.currentURI.spec == getpage(2); }, "getpage(2)");
    yield waitForCondition2(function () { return ContextUI.navbarVisible; }, "ContextUI.navbarVisible");

    is(tab.browser.currentURI.spec, getpage(2), getpage(2));

    yield navForward();
    yield waitForCondition2(function () { return tab.browser.currentURI.spec == getpage(3); }, "getpage(3)");

    is(tab.browser.currentURI.spec, getpage(3), getpage(3));
    ok(ContextUI.navbarVisible, "navbar visible after navigate");

    doEdgeUIGesture();

    is(ContextUI.navbarVisible, true, "Navbar is visible after swipe");
    is(ContextUI.tabbarVisible, true, "Tabbar is visible after swipe");

    yield navBackViaNavButton();
    yield waitForCondition2(function () { return tab.browser.currentURI.spec == getpage(2); }, "getpage(2)");

    is(tab.browser.currentURI.spec, getpage(2), getpage(2));
    is(ContextUI.navbarVisible, true, "Navbar is visible after navigating back (overlay)");
    yield waitForCondition2(function () { return !ContextUI.tabbarVisible; }, "!ContextUI.tabbarVisible");

    sendElementTap(window, window.document.documentElement);
    yield waitForCondition2(function () { return !BrowserUI.navbarVisible; }, "!BrowserUI.navbarVisible");

    is(ContextUI.tabbarVisible, false, "Tabbar is hidden after content tap");

    yield navForward();
    yield waitForCondition2(function () {
      return tab.browser.currentURI.spec == getpage(3) && ContextUI.navbarVisible;
    }, "getpage(3)");

    is(tab.browser.currentURI.spec, getpage(3), getpage(3));
    ok(ContextUI.navbarVisible, "navbar visible after navigate");

    yield navBackViaNavButton();
    yield waitForCondition2(function () { return tab.browser.currentURI.spec == getpage(2); }, "getpage(2)");
    yield waitForCondition2(function () { return !ContextUI.tabbarVisible; }, "!ContextUI.tabbarVisible");

    is(tab.browser.currentURI.spec, getpage(2), getpage(2));
    is(ContextUI.navbarVisible, true, "Navbar is visible after navigating back (overlay)");

    ContextUI.dismiss();

    let note = yield showNotification();
    doEdgeUIGesture();
    sendElementTap(window, note);

    is(ContextUI.navbarVisible, true, "Navbar is visible after clicking notification close button");

    removeNotifications();

    Browser.closeTab(tab, { forceClose: true });
  }
});

gTests.push({
  desc: "Bug 907244 - Opening a new tab when the page has focus doesn't correctly focus the location bar",
  run: function () {
    let mozTab = yield addTab("about:mozilla");

    // addTab will dismiss navbar, but lets check anyway.
    ok(!ContextUI.navbarVisible, "navbar dismissed");

    BrowserUI.doCommand("cmd_newTab");
    let newTab = Browser.selectedTab;
    yield newTab.pageShowPromise;

    yield waitForCondition(() => ContextUI.navbarVisible);
    ok(ContextUI.navbarVisible, "navbar visible");

    let edit = document.getElementById("urlbar-edit");

    ok(edit.focused, "Edit has focus");

    // Lets traverse since node.contains() doesn't work for anonymous elements.
    let parent = document.activeElement;
    while(parent && parent != edit) {
      parent = parent.parentNode;
    }

    ok(parent === edit, 'Active element is a descendant of urlbar');

    Browser.closeTab(newTab, { forceClose: true });
    Browser.closeTab(mozTab, { forceClose: true });
  }
});
