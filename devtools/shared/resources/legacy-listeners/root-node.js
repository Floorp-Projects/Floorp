/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = async function({
  targetList,
  targetType,
  targetFront,
  isTopLevel,
  onAvailable,
}) {
  if (!isTopLevel) {
    return;
  }

  const inspectorFront = await targetFront.getFront("inspector");
  await inspectorFront.walker.watchRootNode(onAvailable);
};
