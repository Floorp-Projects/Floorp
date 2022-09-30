/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

/**
 * This component displays detail information for tab.
 */
class TabDetail extends PureComponent {
  static get propTypes() {
    return {
      target: Types.debugTarget.isRequired,
    };
  }

  render() {
    return dom.div(
      {
        className: "debug-target-item__subname ellipsis-text",
        dir: "ltr",
      },
      this.props.target.details.url
    );
  }
}

module.exports = TabDetail;
