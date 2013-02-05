/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a bad module, it asks for exports but also returns a value from
// the define defintion function.
define(['exports'], function (exports) {    
    return 'badExportAndReturn';
});

