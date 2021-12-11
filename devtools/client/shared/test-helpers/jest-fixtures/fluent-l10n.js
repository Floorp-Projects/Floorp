/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Mock for devtools/client/shared/modules/fluent-l10n/fluent-l10n
 */
class FluentL10n {
  async init() {}

  getBundles() {
    return [];
  }

  getString(id, args) {
    return args ? `${id}__${JSON.stringify(args)}` : id;
  }
}

// Export the class
exports.FluentL10n = FluentL10n;
