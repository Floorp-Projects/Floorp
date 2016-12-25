/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global addMessageListener */

"use strict";

let { classes: Cc, interfaces: Ci } = Components;
let swm = Cc["@mozilla.org/serviceworkers/manager;1"]
  .getService(Ci.nsIServiceWorkerManager);

addMessageListener("serviceWorkerRegistration:start", message => {
  let { data } = message;
  let array = swm.getAllRegistrations();

  // Find the service worker registration with the desired scope.
  for (let i = 0; i < array.length; i++) {
    let registration =
      array.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
    // XXX: In some rare cases, `registration.activeWorker` can be null for a
    // brief moment (e.g. while the service worker is first installing, or if
    // there was an unhandled exception during install that will cause the
    // registration to be removed). We can't do much about it here, simply
    // ignore these cases.
    if (registration.scope === data.scope && registration.activeWorker) {
      // Briefly attaching a debugger to the active service worker will cause
      // it to start running.
      registration.activeWorker.attachDebugger();
      registration.activeWorker.detachDebugger();
      return;
    }
  }
});
