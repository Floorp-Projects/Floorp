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
  if (resource?.pageError?.errorMessage) {
    resource.pageError.errorMessage = getAdHocFrontOrPrimitiveGrip(
      resource.pageError.errorMessage,
      targetFront
    );
  }

  if (resource?.pageError?.exception) {
    resource.pageError.exception = getAdHocFrontOrPrimitiveGrip(
      resource.pageError.exception,
      targetFront
    );
  }

  return resource;
};
