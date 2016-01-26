/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const Browser = createFactory(require("./browser"));

module.exports = createClass({

  displayName: "Viewport",

  propTypes: {
    location: Types.location.isRequired,
    viewport: PropTypes.shape(Types.viewport).isRequired,
  },

  render() {
    let {
      location,
      viewport,
    } = this.props;

    // Additional elements will soon appear here around the Browser, like drag
    // handles, etc.
    return dom.div(
      {
        className: "viewport",
      },
      Browser({
        location,
        width: viewport.width,
        height: viewport.height,
      })
    );
  },

});
