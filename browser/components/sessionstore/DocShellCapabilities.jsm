/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DocShellCapabilities"];

/**
 * The external API exported by this module.
 */
this.DocShellCapabilities = Object.freeze({
  collect(docShell) {
    return DocShellCapabilitiesInternal.collect(docShell);
  },

  restore(docShell, disallow) {
    return DocShellCapabilitiesInternal.restore(docShell, disallow);
  },
});

const CAPABILITIES_TO_IGNORE = new Set(["Javascript"]);

/**
 * Internal functionality to save and restore the docShell.allow* properties.
 */
var DocShellCapabilitiesInternal = {
  // List of docShell capabilities to (re)store. These are automatically
  // retrieved from a given docShell if not already collected before.
  // This is made so they're automatically in sync with all nsIDocShell.allow*
  // properties.
  caps: null,

  allCapabilities(docShell) {
    if (!this.caps) {
      let keys = Object.keys(docShell);
      this.caps = keys.filter(k => k.startsWith("allow")).map(k => k.slice(5));
    }
    return this.caps;
  },

  collect(docShell) {
    let caps = this.allCapabilities(docShell);
    return caps.filter(cap => !docShell["allow" + cap]
                              && !CAPABILITIES_TO_IGNORE.has(cap));
  },

  restore(docShell, disallow) {
    let caps = this.allCapabilities(docShell);
    for (let cap of caps)
      docShell["allow" + cap] = !disallow.has(cap);
  },
};
