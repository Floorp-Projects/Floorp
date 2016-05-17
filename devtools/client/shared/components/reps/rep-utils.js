/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");

  /**
   * Create React factories for given arguments.
   * Example:
   *   const { Rep } = createFactories(require("./rep"));
   */
  function createFactories(args) {
    let result = {};
    for (let p in args) {
      result[p] = React.createFactory(args[p]);
    }
    return result;
  }

  /**
   * Returns true if the given object is a grip (see RDP protocol)
   */
  function isGrip(object) {
    return object && object.actor;
  }

  // Exports from this module
  exports.createFactories = createFactories;
  exports.isGrip = isGrip;
});
