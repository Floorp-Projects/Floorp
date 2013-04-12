/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  runTests();
}

gTests.push({
  desc: "Plugin mouse input",
  run: function() {
    // This test needs "Switch primary and secondary buttons" disabled.
    let origValue = MetroUtils.swapMouseButton(false);
    registerCleanupFunction(function() MetroUtils.swapMouseButton(origValue));

    Services.prefs.setBoolPref("plugin.disable", false);
    Services.prefs.setBoolPref("plugins.click_to_play", false);
    registerCleanupFunction(Services.prefs.clearUserPref.bind(null, "plugin.disable"));
    registerCleanupFunction(Services.prefs.clearUserPref.bind(null, "plugins.click_to_play"));

    let tab = yield addTab(chromeRoot + "browser_plugin_input.html");

    yield hideContextUI();

    let doc = tab.browser.contentDocument;
    let plugin = doc.getElementById("plugin1");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "Plugin activated");

    // XXX: This shouldn't be necessary, but removing it causes the first click to
    //      sometimes not register
    let wait = yield waitForMs(0);
    ok(wait, "Initial wait");

    try {
      is(plugin.getMouseUpEventCount(), 0, "Plugin should not have received "
                                         + "any mouse up events yet.");
    } catch(e) {
      ok(false, "plugin.getMouseUpEventCount should not throw: " + e);
    }

    let bottom = plugin.getBoundingClientRect().height - 1;
    let right = plugin.getBoundingClientRect().width - 1;
    let middleX = right / 2;
    let middleY = bottom / 2;
    let left = 1;
    let top = 1;

    let clicks = [{ x: left, y: top},          // left top corner
                  { x: left, y: middleY},      // left middle
                  { x: left, y: bottom},       // left bottom corner
                  { x: middleX, y: bottom},    // bottom middle
                  { x: right, y: bottom},      // right bottom corner
                  { x: right, y: middleY},     // right middle
                  { x: right, y: top},         // right top corner
                  { x: middleX, y: top},       // top middle
                  { x: middleX, y: middleY}];  // middle

    let curClicks = 0;
    while (clicks.length > 0) {
      let click = clicks.shift();
      curClicks++;
      info("Sending click " + curClicks + " { x: " + click.x + ", y: " + click.y + "}");
      synthesizeNativeMouseLDown(plugin, click.x, click.y);
      synthesizeNativeMouseLUp(plugin, click.x, click.y);
      let success = yield waitForCondition(function() plugin.getMouseUpEventCount() == curClicks);
      ok(success && !(success instanceof Error),
         "Plugin received click " + curClicks);
    }
  }
});
