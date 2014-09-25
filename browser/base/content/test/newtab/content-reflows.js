/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () {
  "use strict";

  const Ci = Components.interfaces;

  docShell.addWeakReflowObserver({
    reflow() {
      // Gather information about the current code path.
      let path = (new Error().stack).split("\n").slice(1).join("\n");
      if (path) {
        sendSyncMessage("newtab-reflow", path);
      }
    },

    reflowInterruptible() {
      // We're not interested in interruptible reflows.
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
                                           Ci.nsISupportsWeakReference])
  });
})();
