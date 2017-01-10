/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  // Allow all cookies, then actually set up the test
  SpecialPowers.pushPrefEnv({"set": [["network.cookie.cookieBehavior", 0]]}, initTest);
}

function initTest() {
    const searchTerm = "example";
    const dummyTerm = "elpmaxe";

    var cm =  Components.classes["@mozilla.org/cookiemanager;1"]
                        .getService(Components.interfaces.nsICookieManager);

    // delete all cookies (might be left over from other tests)
    cm.removeAll();

    // data for cookies
    var vals = [[searchTerm + ".com", dummyTerm, dummyTerm],           // match
                [searchTerm + ".org", dummyTerm, dummyTerm],           // match
                [dummyTerm + ".com", searchTerm, dummyTerm],           // match
                [dummyTerm + ".edu", searchTerm + dummyTerm, dummyTerm], // match
                [dummyTerm + ".net", dummyTerm, searchTerm],           // match
                [dummyTerm + ".org", dummyTerm, searchTerm + dummyTerm], // match
                [dummyTerm + ".int", dummyTerm, dummyTerm]];           // no match

    // matches must correspond to above data
    const matches = 6;

    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
    var cookieSvc = Components.classes["@mozilla.org/cookieService;1"]
                              .getService(Components.interfaces.nsICookieService);
    var v;
    // inject cookies
    for (v in vals) {
        let [host, name, value] = vals[v];
        var cookieUri = ios.newURI("http://" + host);
        cookieSvc.setCookieString(cookieUri, null, name + "=" + value + ";", null);
    }

    // open cookie manager
    var cmd = window.openDialog("chrome://browser/content/preferences/cookies.xul",
                                "Browser:Cookies", "", {});

    // when it has loaded, run actual tests
    cmd.addEventListener("load", function() { executeSoon(function() { runTest(cmd, searchTerm, vals.length, matches); }); }, false);
}

function isDisabled(win, expectation) {
    var disabled = win.document.getElementById("removeAllCookies").disabled;
    is(disabled, expectation, "Remove all cookies button has correct state: " + (expectation ? "disabled" : "enabled"));
}

function runTest(win, searchTerm, cookies, matches) {
    var cm =  Components.classes["@mozilla.org/cookiemanager;1"]
                        .getService(Components.interfaces.nsICookieManager);


    // number of cookies should match injected cookies
    var injectedCookies = 0,
        injectedEnumerator = cm.enumerator;
    while (injectedEnumerator.hasMoreElements()) {
        injectedCookies++;
        injectedEnumerator.getNext();
    }
    is(injectedCookies, cookies, "Number of cookies match injected cookies");

    // "delete all cookies" should be enabled
    isDisabled(win, false);

    // filter cookies and count matches
    win.gCookiesWindow.setFilter(searchTerm);
    is(win.gCookiesWindow._view.rowCount, matches, "Correct number of cookies shown after filter is applied");

    // "delete all cookies" should be enabled
    isDisabled(win, false);


    // select first cookie and delete
    var tree = win.document.getElementById("cookiesList");
    var deleteButton = win.document.getElementById("removeSelectedCookies");
    var rect = tree.treeBoxObject.getCoordsForCellItem(0, tree.columns[0], "cell");
    EventUtils.synthesizeMouse(tree.body, rect.x + rect.width / 2, rect.y + rect.height / 2, {}, win);
    EventUtils.synthesizeMouseAtCenter(deleteButton, {}, win);

    // count cookies should be matches-1
    is(win.gCookiesWindow._view.rowCount, matches - 1, "Deleted selected cookie");

    // select two adjacent cells and delete
    EventUtils.synthesizeMouse(tree.body, rect.x + rect.width / 2, rect.y + rect.height / 2, {}, win);
    var eventObj = {};
    if (navigator.platform.indexOf("Mac") >= 0)
        eventObj.metaKey = true;
    else
        eventObj.ctrlKey = true;
    rect = tree.treeBoxObject.getCoordsForCellItem(1, tree.columns[0], "cell");
    EventUtils.synthesizeMouse(tree.body, rect.x + rect.width / 2, rect.y + rect.height / 2, eventObj, win);
    EventUtils.synthesizeMouseAtCenter(deleteButton, {}, win);

    // count cookies should be matches-3
    is(win.gCookiesWindow._view.rowCount, matches - 3, "Deleted selected two adjacent cookies");

    // "delete all cookies" should be enabled
    isDisabled(win, false);

    // delete all cookies and count
    var deleteAllButton = win.document.getElementById("removeAllCookies");
    EventUtils.synthesizeMouseAtCenter(deleteAllButton, {}, win);
    is(win.gCookiesWindow._view.rowCount, 0, "Deleted all matching cookies");

    // "delete all cookies" should be disabled
    isDisabled(win, true);

    // clear filter and count should be cookies-matches
    win.gCookiesWindow.setFilter("");
    is(win.gCookiesWindow._view.rowCount, cookies - matches, "Unmatched cookies remain");

    // "delete all cookies" should be enabled
    isDisabled(win, false);

    // delete all cookies and count should be 0
    EventUtils.synthesizeMouseAtCenter(deleteAllButton, {}, win);
    is(win.gCookiesWindow._view.rowCount, 0, "Deleted all cookies");

    // check that datastore is also at 0
    var remainingCookies = 0,
        remainingEnumerator = cm.enumerator;
    while (remainingEnumerator.hasMoreElements()) {
        remainingCookies++;
        remainingEnumerator.getNext();
    }
    is(remainingCookies, 0, "Zero cookies remain");

    // "delete all cookies" should be disabled
    isDisabled(win, true);

    // clean up
    win.close();
    finish();
}

