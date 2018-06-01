/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const VisibilityHandler = createFactory(require("devtools/client/shared/components/VisibilityHandler"));
const { connect } = require("devtools/client/shared/vendor/react-redux");

/**
 * This helper is wrapping Redux's connect() method and applying
 * HOC (VisibilityHandler component) on whatever component is
 * originally passed in. The HOC is responsible for not causing
 * rendering if the owner panel runs in the background.
 */
function visibilityHandlerConnect() {
  const args = [].slice.call(arguments);
  return component => {
    return connect(...args)(props => {
      return VisibilityHandler(null, createElement(component, props));
    });
  };
}

module.exports = {
  connect: visibilityHandlerConnect
};
