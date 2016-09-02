/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

add_task(function* test() {
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});

  yield ContentTask.spawn(privateWin.gBrowser.selectedBrowser, {}, function*() {
    let tests = [
      [["https://mozilla.org", content], 1],
      [["https://mozilla.org", "https://example.com"], 0],
    ];
    tests.forEach(item => {
      let sandbox = Components.utils.Sandbox(item[0]);
      let principal = Services.scriptSecurityManager
                              .getSandboxPrincipal(sandbox);
      is(principal.privateBrowsingId, item[1],
         "Sandbox principal should have the correct OriginAttributes");
    });
  });

  privateWin.close();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    let tests = [
      [["https://mozilla.org", content], 0],
      [["https://mozilla.org", "https://example.com"], 0],
    ];
    tests.forEach(item => {
      let sandbox = Components.utils.Sandbox(item[0]);
      let principal = Services.scriptSecurityManager
                              .getSandboxPrincipal(sandbox);
      is(principal.privateBrowsingId, item[1],
         "Sandbox principal should have the correct OriginAttributes");
    });
  });
});
