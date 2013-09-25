/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "TabCrashReporter" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm");

this.TabCrashReporter = {
  init: function () {
    if (this.initialized)
      return;
    this.initialized = true;

    Services.obs.addObserver(this, "ipc:content-shutdown", false);
    Services.obs.addObserver(this, "oop-frameloader-crashed", false);

    this.childMap = new Map();
    this.browserMap = new WeakMap();
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "ipc:content-shutdown":
        aSubject.QueryInterface(Ci.nsIPropertyBag2);

        if (!aSubject.get("abnormal"))
          return;

        this.childMap.set(aSubject.get("childID"), aSubject.get("dumpID"));
        break;

      case "oop-frameloader-crashed":
        aSubject.QueryInterface(Ci.nsIFrameLoader);

        let browser = aSubject.ownerElement;
        if (!browser)
          return;

        this.browserMap.set(browser, aSubject.childID);
        break;
    }
  },

  submitCrashReport: function (aBrowser) {
    let childID = this.browserMap.get(aBrowser);
    let dumpID = this.childMap.get(childID);
    if (!dumpID)
      return

    if (CrashSubmit.submit(dumpID)) {
      this.childMap.set(childID, null); // Avoid resubmission.
    }
  },

  onAboutTabCrashedLoad: function (aBrowser) {
    if (!this.childMap)
      return;

    let dumpID = this.childMap.get(this.browserMap.get(aBrowser));
    if (!dumpID)
      return;

    aBrowser.contentDocument.documentElement.classList.add("crashDumpAvaible");
  }
}
