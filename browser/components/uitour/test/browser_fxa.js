/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

var gTestTab;
var gContentAPI;
var gContentWindow;

function test() {
  UITourTest();
}

registerCleanupFunction(function*() {
  yield signOut();
  gFxAccounts.updateUI();
});

var tests = [
  taskify(function* test_highlight_accountStatus_loggedOut() {
    let userData = yield fxAccounts.getSignedInUser();
    is(userData, null, "Not logged in initially");
    yield showMenuPromise("appMenu");
    yield showHighlightPromise("accountStatus");
    let highlight = document.getElementById("UITourHighlightContainer");
    is(highlight.getAttribute("targetName"), "accountStatus", "Correct highlight target");
  }),

  taskify(function* test_highlight_accountStatus_loggedIn() {
    yield setSignedInUser();
    let userData = yield fxAccounts.getSignedInUser();
    isnot(userData, null, "Logged in now");
    gFxAccounts.updateUI(); // Causes a leak (see bug 1332985)
    yield showMenuPromise("appMenu");
    yield showHighlightPromise("accountStatus");
    let highlight = document.getElementById("UITourHighlightContainer");
    is(highlight.popupBoxObject.anchorNode.id, "PanelUI-fxa-avatar", "Anchored on avatar");
    is(highlight.getAttribute("targetName"), "accountStatus", "Correct highlight target");
  }),
];

// Helpers copied from browser_aboutAccounts.js
// watch out - these will fire observers which if you aren't careful, may
// interfere with the tests.
function setSignedInUser(data) {
  if (!data) {
    data = {
      email: "foo@example.com",
      uid: "1234@lcip.org",
      assertion: "foobar",
      sessionToken: "dead",
      kA: "beef",
      kB: "cafe",
      verified: true
    };
  }
 return fxAccounts.setSignedInUser(data);
}

function signOut() {
  // we always want a "localOnly" signout here...
  return fxAccounts.signOut(true);
}
