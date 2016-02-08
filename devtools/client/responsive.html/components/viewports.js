/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const Viewport = createFactory(require("./viewport"));

module.exports = createClass({

  displayName: "Viewports",

  propTypes: {
    location: Types.location.isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
  },

  render() {
    let {
      location,
      viewports,
    } = this.props;

    return dom.div(
      {
        id: "viewports",
      },
      viewports.map((viewport, index) => {
        return Viewport({
          key: index,
          location,
          viewport,
        });
      })
    );
  },

});
