/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The PageStyle actor flattens the DOM CSS objects a little bit, merging
// Rules and their Styles into one actor. For elements (which have a style
// but no associated rule) we fake a rule with the following style id.
exports.ELEMENT_STYLE = 100;
