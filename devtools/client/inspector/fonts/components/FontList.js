/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Font = createFactory(require("./Font"));

const Types = require("../types");

module.exports = createClass({

  displayName: "FontList",

  propTypes: {
    fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let { fonts } = this.props;

    return dom.div(
      {
        id: "font-container"
      },
      dom.ul(
        {
          id: "all-fonts"
        },
        fonts.map((font, i) => Font({
          key: i,
          font
        }))
      )
    );
  },

});
