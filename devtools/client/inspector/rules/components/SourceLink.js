/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { ELEMENT_STYLE } = require("devtools/client/inspector/rules/constants");

const Types = require("../types");

class SourceLink extends PureComponent {
  static get propTypes() {
    return {
      id: PropTypes.string.isRequired,
      isSourceLinkEnabled: PropTypes.bool.isRequired,
      isUserAgentStyle: PropTypes.bool.isRequired,
      onOpenSourceLink: PropTypes.func.isRequired,
      sourceLink: PropTypes.shape(Types.sourceLink).isRequired,
      type: PropTypes.number.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onSourceClick = this.onSourceClick.bind(this);
  }

  /**
   * Whether or not the source link is enabled. The source link is enabled when the
   * Style Editor is enabled and the rule is not a user agent or element style.
   */
  get isSourceLinkEnabled() {
    return (
      this.props.isSourceLinkEnabled &&
      !this.props.isUserAgentStyle &&
      this.props.type !== ELEMENT_STYLE
    );
  }

  onSourceClick(event) {
    event.stopPropagation();

    if (this.isSourceLinkEnabled) {
      this.props.onOpenSourceLink(this.props.id);
    }
  }

  render() {
    const { label, title } = this.props.sourceLink;

    return dom.div(
      {
        className:
          "ruleview-rule-source theme-link" +
          (!this.isSourceLinkEnabled ? " disabled" : ""),
        onClick: this.onSourceClick,
      },
      dom.span(
        {
          className: "ruleview-rule-source-label",
          title,
        },
        label
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    isSourceLinkEnabled: state.rules.isSourceLinkEnabled,
  };
};

module.exports = connect(mapStateToProps)(SourceLink);
