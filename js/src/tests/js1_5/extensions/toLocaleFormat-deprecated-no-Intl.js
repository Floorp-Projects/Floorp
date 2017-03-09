// |reftest| skip-if(!xulRuntime.shell||this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Don't warn about Date.prototype.toLocaleFormat() when Intl isn't supported.

enableLastWarning();

new Date().toLocaleFormat("%Y");

var warning = getLastWarning();
assertEq(warning, null, "warning shouldn't be emitted for toLocaleFormat");

disableLastWarning();

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
