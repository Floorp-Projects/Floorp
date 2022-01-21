/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1606448 - Shim CoreMetrics Eluminate analytics
 *
 * Sites may rely on eluminate.js tracking in ways which cause breakage,
 * which has been seen on shopping sites such as Vans.com, where the
 * search filtering UX is broken. This shim mitigates such breakage.
 */

if (!window.CM_DDX) {
  window.CM_DDX = {
    domReadyFired: false,
    headScripts: true,
    dispatcherLoadRequested: false,
    firstPassFunctionBinding: false,
    BAD_PAGE_ID_ELAPSED_TIMEOUT: 5000,
    version: -1,
    standalone: false,
    test: {
      syndicate: true,
      testCounter: "",
      doTest: false,
      newWin: false,
      process: () => {},
    },
    partner: {},
    invokeFunctionWhenAvailable: a => {
      a();
    },
    gup: d => "",
    privacy: {
      isDoNotTrackEnabled: () => false,
      setDoNotTrack: () => {},
      getDoNotTrack: () => false,
    },
    setSubCookie: () => {},
  };
  const noopfn = () => {};
  const w = window;
  w.cmAddShared = noopfn;
  w.cmCalcSKUString = noopfn;
  w.cmCreateManualImpressionTag = noopfn;
  w.cmCreateManualLinkClickTag = noopfn;
  w.cmCreateManualPageviewTag = noopfn;
  w.cmCreateOrderTag = noopfn;
  w.cmCreatePageviewTag = noopfn;
  w.cmExecuteTagQueue = noopfn;
  w.cmRetrieveUserID = noopfn;
  w.cmSetClientID = noopfn;
  w.cmSetCurrencyCode = noopfn;
  w.cmSetFirstPartyIDs = noopfn;
  w.cmSetSubCookie = noopfn;
  w.cmSetupCookieMigration = noopfn;
  w.cmSetupNormalization = noopfn;
  w.cmSetupOther = noopfn;
  w.cmStartTagSet = noopfn;
}
