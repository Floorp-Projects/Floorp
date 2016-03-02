/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// For bug 773980, test that Components.utils.isDeadWrapper works as expected.

add_task(function* test() {
  const url = "http://mochi.test:8888/browser/js/xpconnect/tests/browser/browser_deadObjectOnUnload.html";
  let newTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;
  let contentDocDead = yield ContentTask.spawn(browser,{}, function*(browser){
    let doc = content.document;
    let promise = ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", true);
    content.location = "about:home";
    yield promise;
    return Components.utils.isDeadWrapper(doc);
  });
  is(contentDocDead, true, "wrapper is dead");
  yield BrowserTestUtils.removeTab(newTab); 
});
