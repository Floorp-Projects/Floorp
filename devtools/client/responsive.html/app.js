/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, createFactory, PropTypes, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { resizeViewport, rotateViewport } = require("./actions/viewports");
const Types = require("./types");
const Viewports = createFactory(require("./components/viewports"));
const GlobalToolbar = createFactory(require("./components/global-toolbar"));

let App = createClass({

  displayName: "App",

  propTypes: {
    location: Types.location.isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    onExit: PropTypes.func.isRequired,
  },

  render() {
    let {
      dispatch,
      location,
      viewports,
      onExit,
    } = this.props;

    return dom.div(
      {
        id: "app",
      },
      GlobalToolbar({
        onExit,
      }),
      Viewports({
        location,
        viewports,
        onRotateViewport: id => dispatch(rotateViewport(id)),
        onResizeViewport: (id, width, height) =>
          dispatch(resizeViewport(id, width, height)),
      })
    );
  },

});

module.exports = connect(state => state)(App);
