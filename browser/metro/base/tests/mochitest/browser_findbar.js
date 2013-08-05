/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  runTests();
}

gTests.push({
  desc: "Access the find bar with the keyboard",
  run: function() {
    let tab = yield addTab(chromeRoot + "browser_findbar.html");
    yield waitForCondition(() => BrowserUI.ready);
    is(Elements.findbar.isShowing, false, "Find bar is hidden by default");

    EventUtils.synthesizeKey("f", { accelKey: true });
    yield waitForEvent(Elements.findbar, "transitionend");
    is(Elements.findbar.isShowing, true, "Show find bar with Ctrl-F");

    let textbox = document.getElementById("findbar-textbox");
    is(textbox.value, "", "Find bar is empty");

    EventUtils.sendString("bar");
    is(textbox.value, "bar", "Type 'bar' into find bar");

    EventUtils.synthesizeKey("VK_ESCAPE", { accelKey: true });
    yield waitForEvent(Elements.findbar, "transitionend");
    is(Elements.findbar.isShowing, false, "Hide find bar with Esc");

    Browser.closeTab(tab);
  }
});

gTests.push({
  desc: "Findbar/navbar interaction",
  run: function() {
    let tab = yield addTab(chromeRoot + "browser_findbar.html");
    yield waitForCondition(() => BrowserUI.ready);
    is(ContextUI.navbarVisible, false, "Navbar is hidden by default");
    is(Elements.findbar.isShowing, false, "Find bar is hidden by default");

    yield showNavBar();
    is(ContextUI.navbarVisible, true, "Navbar is visible");
    is(Elements.findbar.isShowing, false, "Find bar is still hidden");

    EventUtils.synthesizeKey("f", { accelKey: true });
    yield Promise.all(waitForEvent(Elements.navbar, "transitionend"),
                      waitForEvent(Elements.findbar, "transitionend"));
    is(ContextUI.navbarVisible, false, "Navbar is hidden");
    is(Elements.findbar.isShowing, true, "Findbar is visible");

    yield Promise.all(showNavBar(),
                      waitForEvent(Elements.findbar, "transitionend"));
    is(ContextUI.navbarVisible, true, "Navbar is visible again");
    is(Elements.findbar.isShowing, false, "Find bar is hidden again");

    Browser.closeTab(tab);
  }
});


gTests.push({
  desc: "Show and hide the find bar with mouse",
  run: function() {
    let tab = yield addTab(chromeRoot + "browser_findbar.html");
    yield waitForCondition(() => BrowserUI.ready);
    is(Elements.findbar.isShowing, false, "Find bar is hidden by default");

    yield showNavBar();
    EventUtils.sendMouseEvent({ type: "click" }, "menu-button");
    EventUtils.sendMouseEvent({ type: "click" }, "context-findinpage");
    yield waitForEvent(Elements.findbar, "transitionend");
    is(Elements.findbar.isShowing, true, "Show find bar with menu item");

    EventUtils.synthesizeMouse(document.getElementById("findbar-close-button"), 1, 1, {});
    yield waitForEvent(Elements.findbar, "transitionend");
    is(Elements.findbar.isShowing, false, "Hide find bar with close button");

    Browser.closeTab(tab);
  }
});
