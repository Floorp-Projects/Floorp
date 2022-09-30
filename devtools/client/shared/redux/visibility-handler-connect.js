/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("resource://devtools/client/shared/vendor/react.js");
const VisibilityHandler = createFactory(
  require("resource://devtools/client/shared/components/VisibilityHandler.js")
);
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

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
  connect: visibilityHandlerConnect,
};
