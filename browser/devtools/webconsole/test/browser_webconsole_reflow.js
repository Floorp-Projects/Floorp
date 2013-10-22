/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test()
{
  addTab("data:text/html;charset=utf-8,Web Console test for reflow activity");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(gBrowser.selectedTab, function(hud) {

      function onReflowListenersReady(aType, aPacket) {
        browser.contentDocument.body.style.display = "none";
        browser.contentDocument.body.clientTop;
      }

      Services.prefs.setBoolPref("devtools.webconsole.filter.csslog", true);
      hud.ui._updateReflowActivityListener(onReflowListenersReady);
      Services.prefs.clearUserPref("devtools.webconsole.filter.csslog");

      waitForMessages({
        webconsole: hud,
        messages: [{
          text: /reflow: /,
          category: CATEGORY_CSS,
          severity: SEVERITY_LOG,
        }],
      }).then(() => {
        finishTest();
      });
    });
  }, true);
}
