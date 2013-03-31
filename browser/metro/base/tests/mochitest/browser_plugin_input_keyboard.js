/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  runTests();
}

gTests.push({
  desc: "Plugin keyboard input",
  run: function() {
    Services.prefs.setBoolPref("plugin.disable", false);
    Services.prefs.setBoolPref("plugins.click_to_play", false);
    registerCleanupFunction(Services.prefs.clearUserPref.bind(null, "plugin.disable"));
    registerCleanupFunction(Services.prefs.clearUserPref.bind(null, "plugins.click_to_play"));

    let tab = yield addTab(chromeRoot + "browser_plugin_input.html");

    let doc = tab.browser.contentDocument;
    let plugin = doc.getElementById("plugin1");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "Plugin activated");
    plugin.focus();

    try {
      is(plugin.getLastKeyText(), "", "Plugin should not have received "
                                    + "any character events yet.");
    } catch(e) {
      ok(false, "plugin.getLastKeyText should not throw: " + e);
    }

    let keys = [{ kbLayout: arSpanish,
                  keyCode: 65,
                  modifiers: 0,
                  expectedChar: 'a' }];

    /* XXX: Re-enable this once bug 837293 is fixed
                { kbLayout: arSpanish,
                  keyCode: 65,
                  modifiers: rightAlt,
                  expectedChar: "á" }];
    */

    while (keys.length > 0) {
      let key = keys.shift();
      info("Sending keypress: " + key.expectedChar);
      synthesizeNativeKey(key.kbLayout, key.keyCode, key.modifiers);
      let success = yield waitForCondition(function() plugin.getLastKeyText() == key.expectedChar);
      ok(success && !(success instanceof Error),
         "Plugin received char: " + key.expectedChar);
    }
  }
});
