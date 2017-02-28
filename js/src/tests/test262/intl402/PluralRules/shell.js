// file: test262-intl-extras.js
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Call the shell helper to add experimental features to the Intl object.
if (typeof addIntlExtras === "function") {
    let intlExtras = {};
    addIntlExtras(intlExtras);

    Object.defineProperty(Intl, "PluralRules", {
        value: intlExtras.PluralRules,
        writable: true, enumerable: false, configurable: true
    });
}
