/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillUtils"];

this.FormAutofillUtils = {
  generateFullName(firstName, lastName, middleName) {
    // TODO: The implementation should depend on the L10N spec, but a simplified
    // rule is used here.
    let fullName = firstName;
    if (middleName) {
      fullName += " " + middleName;
    }
    if (lastName) {
      fullName += " " + lastName;
    }
    return fullName;
  },
};
