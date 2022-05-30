/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1771557 - Shim DoubleVerify analytics
 *
 * Some sites such as Sports Illustrated expect DoubleVerify's
 * analytics script to load, otherwise odd breakage may occur.
 * This shim helps mitigate such breakage.
 */

if (!window?.PQ?.loaded) {
  const cmd = [];
  cmd.push = function(c) {
    try {
      c?.();
    } catch (_) {}
  };

  window.apntag = {
    anq: [],
  };

  window.PQ = {
    cmd,
    loaded: true,
    getTargeting: (_, cb) => cb?.([]),
    init: () => {},
    loadSignals: (_, cb) => cb?.(),
    loadSignalsForSlots: (_, cb) => cb?.(),
    PTS: {},
  };
}
