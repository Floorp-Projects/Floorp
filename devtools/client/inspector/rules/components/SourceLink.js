/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

class SourceLink extends PureComponent {
  static get propTypes() {
    return {
      sourceLink: PropTypes.shape(Types.sourceLink).isRequired,
    };
  }

  render() {
    const { sourceLink } = this.props;

    return (
      dom.div({ className: "ruleview-rule-source theme-link" },
        dom.span({ className: "ruleview-rule-source-label" },
          sourceLink.title
        )
      )
    );
  }
}

module.exports = SourceLink;
