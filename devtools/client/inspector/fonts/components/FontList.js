/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM: dom,
  PropTypes,
  PureComponent,
} = require("devtools/client/shared/vendor/react");

const Font = createFactory(require("./Font"));

const Types = require("../types");

class FontList extends PureComponent {
  static get propTypes() {
    return {
      fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired
    };
  }

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
  }
}

module.exports = FontList;
