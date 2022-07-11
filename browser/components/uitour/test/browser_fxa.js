/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyGetter(this, "fxAccounts", () => {
  return ChromeUtils.import(
    "resource://gre/modules/FxAccounts.jsm"
  ).getFxAccountsSingleton();
});

var gTestTab;
var gContentAPI;

function test() {
  UITourTest();
}

const oldState = UIState.get();
registerCleanupFunction(async function() {
  await signOut();
  gSync.updateAllUI(oldState);
});

var tests = [
  taskify(async function test_highlight_accountStatus_loggedOut() {
    await showMenuPromise("appMenu");
    await showHighlightPromise("accountStatus");
    let highlight = document.getElementById("UITourHighlightContainer");
    is(
      highlight.getAttribute("targetName"),
      "accountStatus",
      "Correct highlight target"
    );
  }),

  taskify(async function test_highlight_accountStatus_loggedIn() {
    gSync.updateAllUI({
      status: UIState.STATUS_SIGNED_IN,
      lastSync: new Date(),
      email: "foo@example.com",
    });
    await showMenuPromise("appMenu");
    await showHighlightPromise("accountStatus");
    let highlight = document.getElementById("UITourHighlightContainer");
    is(
      highlight.getAttribute("targetName"),
      "accountStatus",
      "Correct highlight target"
    );
  }),
];

function signOut() {
  // we always want a "localOnly" signout here...
  return fxAccounts.signOut(true);
}
