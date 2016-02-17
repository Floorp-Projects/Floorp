/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { resizeViewport, rotateViewport } = require("./actions/viewports");
const Types = require("./types");
const Viewports = createFactory(require("./components/viewports"));

let App = createClass({

  displayName: "App",

  propTypes: {
    location: Types.location.isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
  },

  render() {
    let {
      dispatch,
      location,
      viewports,
    } = this.props;

    // For the moment, the app is just the viewports.  This seems likely to
    // change assuming we add a global toolbar or something similar.
    return Viewports({
      location,
      viewports,
      onRotateViewport: id => dispatch(rotateViewport(id)),
      onResizeViewport: (id, width, height) =>
        dispatch(resizeViewport(id, width, height)),
    });
  },

});

module.exports = connect(state => state)(App);
