/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function retrieveUserContextId(browser) {
  return ContentTask.spawn(browser, null, function* () {
    return docShell.userContextId;
  });
}

add_task(function() {
  for (let i = 0; i < 3; ++i) {
    let tab = gBrowser.addTab("about:blank");
    let browser = tab.linkedBrowser;

    yield promiseBrowserLoaded(browser);
    yield promiseTabState(tab, { userContextId: i, entries: [{ url: "http://example.com/" }] });

    let userContextId = yield retrieveUserContextId(browser);
    is(userContextId, i, "The docShell has the correct userContextId");

    yield promiseRemoveTab(tab);
  }
});
