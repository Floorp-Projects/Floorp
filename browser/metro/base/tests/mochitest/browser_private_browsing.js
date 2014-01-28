/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  runTests();
}

gTests.push({
  desc: "Private tab sanity check",
  run: function() {
    let tab = Browser.addTab("about:mozilla");
    is(tab.isPrivate, false, "Tabs are not private by default");
    is(tab.chromeTab.hasAttribute("private"), false,
      "non-private tab has no private attribute");
    Browser.closeTab(tab, { forceClose: true });

    tab = Browser.addTab("about:mozilla", false, null, { private: true });
    is(tab.isPrivate, true, "Create a private tab");
    is(tab.chromeTab.getAttribute("private"), "true",
      "private tab has private attribute");
    Browser.closeTab(tab, { forceClose: true });
  }
});
