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

  _renderNoIssues() {
    return dom.div(
      { className: "devtools-sidepanel-no-result" },
      "No compatibility issues found."
    );
  }

  render() {
    const { selectedNodeIssues } = this.props;

    return dom.div(
      {
        className: "compatibility-app theme-sidebar inspector-tabpanel",
      },
      selectedNodeIssues.length
        ? IssueList({ issues: selectedNodeIssues })
        : this._renderNoIssues()
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedNodeIssues: state.compatibility.selectedNodeIssues,
  };
};
module.exports = connect(mapStateToProps)(CompatibilityApp);
