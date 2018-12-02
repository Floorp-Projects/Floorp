// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// In locales that don't have a relative-date/time formatter -- and presently
// "ak" is such a locale -- behavior is expected to fall back to the root-locale
// formatter.  This test verifies such fallback works as long as "ak" satisfies
// these properties; "ak" may safely be changed to a different locale if that
// ever changes.  See bug 1504656.
assertEq(new Intl.RelativeTimeFormat("ak").format(1, "second"),
         "+1 s");

if (typeof reportCompare === "function")
  reportCompare(0, 0, 'ok');
