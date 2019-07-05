/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported addressDataExt */
/* eslint max-len: 0 */

"use strict";

// "addressDataExt" uses the same key as "addressData" in "addressReferences.js" and
//  contains the information we need but absent in "libaddressinput" such as alternative names.

// TODO: We only support the alternative name of US in MVP. We are going to support more countries in
//       bug 1370193.
var addressDataExt = {
  "data/US": {
    alternative_names: [
      "US",
      "United States of America",
      "United States",
      "America",
      "U.S.",
      "USA",
      "U.S.A.",
      "U.S.A",
    ],
    fmt: "%N%n%A%n%C%S%n%Z%O",
  },
};
