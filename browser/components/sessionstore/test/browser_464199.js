/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/ForgetAboutSite.jsm");

function waitForClearHistory(aCallback) {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "browser:purge-domain-data");
      setTimeout(aCallback, 0);
    }
  };
  Services.obs.addObserver(observer, "browser:purge-domain-data", false);
}

function test() {
  /** Test for Bug 464199 **/

  waitForExplicitFinish();

  const REMEMBER = Date.now(), FORGET = Math.random();
  let test_state = { windows: [{ "tabs": [{ "entries": [] }], _closedTabs: [
    { state: { entries: [{ url: "http://www.example.net/" }] }, title: FORGET },
    { state: { entries: [{ url: "http://www.example.org/" }] }, title: REMEMBER },
    { state: { entries: [{ url: "http://www.example.net/" },
                         { url: "http://www.example.org/" }] }, title: FORGET },
    { state: { entries: [{ url: "http://example.net/" }] }, title: FORGET },
    { state: { entries: [{ url: "http://sub.example.net/" }] }, title: FORGET },
    { state: { entries: [{ url: "http://www.example.net:8080/" }] }, title: FORGET },
    { state: { entries: [{ url: "about:license" }] }, title: REMEMBER },
    { state: { entries: [{ url: "http://www.example.org/frameset",
                           children: [
                             { url: "http://www.example.org/frame" },
                             { url: "http://www.example.org:8080/frame2" }
                           ] }] }, title: REMEMBER },
    { state: { entries: [{ url: "http://www.example.org/frameset",
                           children: [
                             { url: "http://www.example.org/frame" },
                             { url: "http://www.example.net/frame" }
                           ] }] }, title: FORGET },
    { state: { entries: [{ url: "http://www.example.org/form",
                           formdata: { id: { "url": "http://www.example.net/" } }
                         }] }, title: REMEMBER },
    { state: { entries: [{ url: "http://www.example.org/form" }],
               extData: { "setTabValue": "http://example.net:80" } }, title: REMEMBER }
  ] }] };
  let remember_count = 5;

  function countByTitle(aClosedTabList, aTitle) {
    return aClosedTabList.filter(aData => aData.title == aTitle).length;
  }

  // open a window and add the above closed tab list
  let newWin = openDialog(location, "", "chrome,all,dialog=no");
  promiseWindowLoaded(newWin).then(() => {
    gPrefService.setIntPref("browser.sessionstore.max_tabs_undo",
                            test_state.windows[0]._closedTabs.length);
    ss.setWindowState(newWin, JSON.stringify(test_state), true);

    let closedTabs = JSON.parse(ss.getClosedTabData(newWin));
    is(closedTabs.length, test_state.windows[0]._closedTabs.length,
       "Closed tab list has the expected length");
    is(countByTitle(closedTabs, FORGET),
       test_state.windows[0]._closedTabs.length - remember_count,
       "The correct amout of tabs are to be forgotten");
    is(countByTitle(closedTabs, REMEMBER), remember_count,
       "Everything is set up.");

    ForgetAboutSite.removeDataFromDomain("example.net");
    waitForClearHistory(function() {
      closedTabs = JSON.parse(ss.getClosedTabData(newWin));
      is(closedTabs.length, remember_count,
         "The correct amout of tabs was removed");
      is(countByTitle(closedTabs, FORGET), 0,
         "All tabs to be forgotten were indeed removed");
      is(countByTitle(closedTabs, REMEMBER), remember_count,
         "... and tabs to be remembered weren't.");

      // clean up
      newWin.close();
      gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
     finish();
    });
  });
}
