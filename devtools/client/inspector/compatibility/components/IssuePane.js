/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

const IssueList = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/IssueList.js")
);

class IssuePane extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      issues: PropTypes.arrayOf(PropTypes.shape(Types.issue)).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
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
    const { dispatch, issues, setSelectedNode } = this.props;

    return issues.length
      ? IssueList({
          dispatch,
          issues,
          setSelectedNode,
        })
      : this._renderNoIssues();
  }
}

module.exports = IssuePane;
