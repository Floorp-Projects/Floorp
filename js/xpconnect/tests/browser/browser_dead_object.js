/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// For bug 773980, test that Components.utils.isDeadWrapper works as expected.

add_task(async function test() {
  const url = "http://mochi.test:8888/browser/js/xpconnect/tests/browser/browser_deadObjectOnUnload.html";
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;
  let innerWindowId = browser.innerWindowID;
  let contentDocDead = await ContentTask.spawn(browser,{innerWindowId}, async function(args){
    let doc = content.document;
    let {TestUtils} = Components.utils.import("resource://testing-common/TestUtils.jsm", {});
    let promise = TestUtils.topicObserved("inner-window-nuked", (subject, data) => {
      let id = subject.QueryInterface(Components.interfaces.nsISupportsPRUint64).data;
      return id == args.innerWindowId;
    });
    content.location = "about:home";
    await promise;
    return Components.utils.isDeadWrapper(doc);
  });
  is(contentDocDead, true, "wrapper is dead");
  await BrowserTestUtils.removeTab(newTab); 
});
