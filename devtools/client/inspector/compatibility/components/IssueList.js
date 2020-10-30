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

const Types = require("devtools/client/inspector/compatibility/types");

const IssueItem = createFactory(
  require("devtools/client/inspector/compatibility/components/IssueItem")
);

class IssueList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      issues: PropTypes.arrayOf(PropTypes.shape(Types.issue)).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const { dispatch, issues, setSelectedNode } = this.props;

    return dom.ul(
      { className: "compatibility-issue-list" },
      issues.map(issue =>
        IssueItem({
          ...issue,
          dispatch,
          setSelectedNode,
        })
      )
    );
  }
}

module.exports = IssueList;
