/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// eslint-disable-next-line mozilla/reject-some-requires
loader.lazyRequireGetter(
  this,
  "getAdHocFrontOrPrimitiveGrip",
  "devtools/client/fronts/object",
  true
);

module.exports = function({ resource, targetFront }) {
  if (Array.isArray(resource.message.arguments)) {
    // We might need to create fronts for each of the message arguments.
    resource.message.arguments = resource.message.arguments.map(arg =>
      getAdHocFrontOrPrimitiveGrip(arg, targetFront)
    );
  }
  return resource;
};
