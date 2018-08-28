/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { PureComponent } = require("devtools/client/shared/vendor/react");

/**
 * This class provides the shape of indication bar such as scrubber and progress bar.
 * Also, make the bar to move to correct position even resizing animation inspector.
 */
class IndicationBar extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string.isRequired,
      position: PropTypes.number.isRequired,
    };
  }

  render() {
    const { className, position } = this.props;

    return dom.div(
      {
        className: `indication-bar ${ className }`,
        style: {
          marginInlineStart: `${ position * 100 }%`,
        },
      }
    );
  }
}

module.exports = IndicationBar;
