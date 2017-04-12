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
   *   const {
   *     Tabs,
   *     TabPanel
   *   } = createFactories(require("devtools/client/shared/components/tabs/tabs"));
   */
  function createFactories(args) {
    let result = {};
    for (let p in args) {
      result[p] = React.createFactory(args[p]);
    }
    return result;
  }

  module.exports = {
    createFactories,
  };
});
