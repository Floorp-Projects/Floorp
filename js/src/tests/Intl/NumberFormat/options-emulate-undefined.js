// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// objectEmulatingUndefined is only available when running tests in the shell,
// not the browser
if (typeof objectEmulatingUndefined === "function") {
  let nf = new Intl.NumberFormat('en-US', objectEmulatingUndefined());
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
