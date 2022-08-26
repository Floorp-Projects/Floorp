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

const FluentReact = require("devtools/client/shared/vendor/fluent-react");

const Types = require("devtools/client/inspector/compatibility/types");

const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const Footer = createFactory(
  require("devtools/client/inspector/compatibility/components/Footer")
);
const IssuePane = createFactory(
  require("devtools/client/inspector/compatibility/components/IssuePane")
);
const Settings = createFactory(
  require("devtools/client/inspector/compatibility/components/Settings")
);

class CompatibilityApp extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      // getString prop is injected by the withLocalization wrapper
      getString: PropTypes.func.isRequired,
      isSettingsVisibile: PropTypes.bool.isRequired,
      isTopLevelTargetProcessing: PropTypes.bool.isRequired,
      selectedNodeIssues: PropTypes.arrayOf(PropTypes.shape(Types.issue))
        .isRequired,
      topLevelTargetIssues: PropTypes.arrayOf(PropTypes.shape(Types.issue))
        .isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      dispatch,
      getString,
      isSettingsVisibile,
      isTopLevelTargetProcessing,
      selectedNodeIssues,
      topLevelTargetIssues,
      setSelectedNode,
    } = this.props;

    const selectedNodeIssuePane = IssuePane({
      issues: selectedNodeIssues,
    });

    const topLevelTargetIssuePane =
      topLevelTargetIssues.length || !isTopLevelTargetProcessing
        ? IssuePane({
            dispatch,
            issues: topLevelTargetIssues,
            setSelectedNode,
          })
        : null;

    const throbber = isTopLevelTargetProcessing
      ? dom.div({
          className: "compatibility-app__throbber devtools-throbber",
        })
      : null;

    return dom.section(
      {
        className: "compatibility-app theme-sidebar inspector-tabpanel",
      },
      dom.div(
        {
          className:
            "compatibility-app__container" +
            (isSettingsVisibile ? " compatibility-app__container-hidden" : ""),
        },
        Accordion({
          className: "compatibility-app__main",
          items: [
            {
              id: "compatibility-app--selected-element-pane",
              header: getString("compatibility-selected-element-header"),
              component: selectedNodeIssuePane,
              opened: true,
            },
            {
              id: "compatibility-app--all-elements-pane",
              header: getString("compatibility-all-elements-header"),
              component: [topLevelTargetIssuePane, throbber],
              opened: true,
            },
          ],
        }),
        Footer({
          className: "compatibility-app__footer",
        })
      ),
      isSettingsVisibile ? Settings() : null
    );
  }
}

const mapStateToProps = state => {
  return {
    isSettingsVisibile: state.compatibility.isSettingsVisibile,
    isTopLevelTargetProcessing: state.compatibility.isTopLevelTargetProcessing,
    selectedNodeIssues: state.compatibility.selectedNodeIssues,
    topLevelTargetIssues: state.compatibility.topLevelTargetIssues,
  };
};
module.exports = FluentReact.withLocalization(
  connect(mapStateToProps)(CompatibilityApp)
);
