/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  runTests();
}

gTests.push({
  desc: "Onscreen keyboard tests",
  run: function() {
    // By design, Metro apps can't show the keyboard programmatically, so we
    // can't use the real keyboard in this test:
    // http://msdn.microsoft.com/en-us/library/windows/apps/hh465404.aspx#user-driven_invocation
    //
    // Instead, we will use this mock object to simulate keyboard changes.
    let originalUtils = Services.metro;
    Services.metro = {
      keyboardHeight: 0,
      keyboardVisible: false
    };
    registerCleanupFunction(function() {
      Services.metro = originalUtils;
    });

    let tab = yield addTab(chromeRoot + "browser_onscreen_keyboard.html");
    // Explicitly dismiss the toolbar at the start, because it messes with the
    // keyboard and sizing if it's dismissed later.
    ContextUI.dismiss();

    let doc = tab.browser.contentDocument;
    let text = doc.getElementById("text")
    let rect0 = text.getBoundingClientRect();
    let rect0browserY = Math.floor(tab.browser.ptClientToBrowser(rect0.left, rect0.top).y);

    // Simulate touch
    SelectionHelperUI.attachToCaret(tab.browser, rect0.left + 5, rect0.top + 5);

    // "Show" the keyboard.
    Services.metro.keyboardHeight = 100;
    Services.metro.keyboardVisible = true;
    Services.obs.notifyObservers(null, "metro_softkeyboard_shown", null);

    let event = yield waitForEvent(window, "MozDeckOffsetChanged");
    is(event.detail, 100, "deck offset by keyboard height");

    let rect1 = text.getBoundingClientRect();
    let rect1browserY = Math.floor(tab.browser.ptClientToBrowser(rect1.left, rect1.top).y);
    is(rect1browserY, rect0browserY + 100, "text field moves up by 100px");

    // "Hide" the keyboard.
    Services.metro.keyboardHeight = 0;
    Services.metro.keyboardVisible = false;
    Services.obs.notifyObservers(null, "metro_softkeyboard_hidden", null);

    yield waitForEvent(window, "MozDeckOffsetChanged");

    finish();
  }
});
