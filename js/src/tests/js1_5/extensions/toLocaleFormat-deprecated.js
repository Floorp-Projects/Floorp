// |reftest| skip-if(!xulRuntime.shell||!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Warn once about Date.prototype.toLocaleFormat().

enableLastWarning();

new Date().toLocaleFormat("%Y");

var warning = getLastWarning();
assertEq(warning !== null, true, "warning should be emitted for toLocaleFormat");
assertEq(warning.name, "Warning");
assertEq(warning.message.indexOf("toLocaleFormat") !== -1, true,
         "warning should mention toLocaleFormat");

clearLastWarning();

new Date().toLocaleFormat("%Y");

warning = getLastWarning();
assertEq(warning, null, "warning shouldn't be emitted for 2nd call to toLocaleFormat");

disableLastWarning();

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
