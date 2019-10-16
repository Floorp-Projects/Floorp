/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

const IssueList = createFactory(require("./IssueList"));

class CompatibilityApp extends PureComponent {
  static get propTypes() {
    return {
      selectedNodeIssues: PropTypes.arrayOf(PropTypes.shape(Types.issue))
        .isRequired,
    };
  }

  render() {
    const { selectedNodeIssues } = this.props;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
      },
      selectedNodeIssues.length
        ? IssueList({ issues: selectedNodeIssues })
        : null
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedNodeIssues: state.compatibility.selectedNodeIssues,
  };
};
module.exports = connect(mapStateToProps)(CompatibilityApp);
