/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

const Accordion = createFactory(
  require("resource://devtools/client/shared/components/Accordion.js")
);
const Footer = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/Footer.js")
);
const IssuePane = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/IssuePane.js")
);
const Settings = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/Settings.js")
);

class CompatibilityApp extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      // getString prop is injected by the withLocalization wrapper
      getString: PropTypes.func.isRequired,
      isSettingsVisible: PropTypes.bool.isRequired,
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
      isSettingsVisible,
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
            (isSettingsVisible ? " compatibility-app__container-hidden" : ""),
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
      isSettingsVisible ? Settings() : null
    );
  }
}

const mapStateToProps = state => {
  return {
    isSettingsVisible: state.compatibility.isSettingsVisible,
    isTopLevelTargetProcessing: state.compatibility.isTopLevelTargetProcessing,
    selectedNodeIssues: state.compatibility.selectedNodeIssues,
    topLevelTargetIssues: state.compatibility.topLevelTargetIssues,
  };
};
module.exports = FluentReact.withLocalization(
  connect(mapStateToProps)(CompatibilityApp)
);
