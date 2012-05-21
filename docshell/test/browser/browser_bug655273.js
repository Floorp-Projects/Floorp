/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 655273.  Make sure that after changing the URI via
 * history.pushState, the resulting SHEntry has the same title as our old
 * SHEntry.
 **/

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab('http://example.com');
  let tabBrowser = tab.linkedBrowser;

  tabBrowser.addEventListener('load', function(aEvent) {
    tabBrowser.removeEventListener('load', arguments.callee, true);

    let cw = tabBrowser.contentWindow;
    let oldTitle = cw.document.title;
    ok(oldTitle, 'Content window should initially have a title.');
    cw.history.pushState('', '', 'new_page');

    let shistory = cw.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .sessionHistory;

    is(shistory.getEntryAtIndex(shistory.index, false).title,
       oldTitle, 'SHEntry title after pushstate.');

    gBrowser.removeTab(tab);
    finish();
  }, true);
}
