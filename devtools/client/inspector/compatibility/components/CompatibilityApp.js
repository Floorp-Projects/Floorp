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

const Types = require("devtools/client/inspector/compatibility/types");

const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const IssuePane = createFactory(
  require("devtools/client/inspector/compatibility/components/IssuePane")
);

class CompatibilityApp extends PureComponent {
  static get propTypes() {
    return {
      selectedNodeIssues: PropTypes.arrayOf(PropTypes.shape(Types.issue))
        .isRequired,
      topLevelTargetIssues: PropTypes.arrayOf(PropTypes.shape(Types.issue))
        .isRequired,
      hideBoxModelHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelHighlighterForNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      selectedNodeIssues,
      topLevelTargetIssues,
      hideBoxModelHighlighter,
      setSelectedNode,
      showBoxModelHighlighterForNode,
    } = this.props;

    return dom.section(
      {
        className: "compatibility-app theme-sidebar inspector-tabpanel",
      },
      Accordion({
        items: [
          {
            id: "compatibility-app--selected-element-pane",
            header: "Selected Element",
            component: IssuePane,
            componentProps: {
              issues: selectedNodeIssues,
            },
            opened: true,
          },
          {
            id: "compatibility-app--all-elements-pane",
            header: "All Issues",
            component: IssuePane,
            componentProps: {
              issues: topLevelTargetIssues,
              hideBoxModelHighlighter,
              setSelectedNode,
              showBoxModelHighlighterForNode,
            },
            opened: true,
          },
        ],
      })
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedNodeIssues: state.compatibility.selectedNodeIssues,
    topLevelTargetIssues: state.compatibility.topLevelTargetIssues,
  };
};
module.exports = connect(mapStateToProps)(CompatibilityApp);
