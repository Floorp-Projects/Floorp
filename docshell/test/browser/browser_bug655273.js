/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 655273.  Make sure that after changing the URI via
 * history.pushState, the resulting SHEntry has the same title as our old
 * SHEntry.
 **/

add_task(async function test() {
  waitForExplicitFinish();

  await BrowserTestUtils.withNewTab({ gBrowser, url: "http://example.com" },
    async function(browser) {
      await ContentTask.spawn(browser, null, async function() {
        let cw = content;
        let oldTitle = cw.document.title;
        ok(oldTitle, "Content window should initially have a title.");
        cw.history.pushState("", "", "new_page");

        let shistory = cw.docShell
                         .QueryInterface(Ci.nsIWebNavigation)
                         .sessionHistory;

        is(shistory.legacySHistory.getEntryAtIndex(shistory.index).title,
           oldTitle, "SHEntry title after pushstate.");
      });
    });
});
