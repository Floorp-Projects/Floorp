/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Types = require("devtools/client/inspector/compatibility/types");

const IssueList = createFactory(
  require("devtools/client/inspector/compatibility/components/IssueList")
);

class IssuePane extends PureComponent {
  static get propTypes() {
    return {
      issues: PropTypes.arrayOf(PropTypes.shape(Types.issue)).isRequired,
      hideBoxModelHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelHighlighterForNode: PropTypes.func.isRequired,
    };
  }

  _renderNoIssues() {
    return Localized(
      { id: "compatibility-no-issues-found" },
      dom.p(
        { className: "devtools-sidepanel-no-result" },
        "compatibility-no-issues-found"
      )
    );
  }

  render() {
    const {
      issues,
      hideBoxModelHighlighter,
      setSelectedNode,
      showBoxModelHighlighterForNode,
    } = this.props;

    return issues.length
      ? IssueList({
          issues,
          hideBoxModelHighlighter,
          setSelectedNode,
          showBoxModelHighlighterForNode,
        })
      : this._renderNoIssues();
  }
}

module.exports = IssuePane;
