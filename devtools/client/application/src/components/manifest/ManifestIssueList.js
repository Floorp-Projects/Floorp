/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const { ul } = require("devtools/client/shared/vendor/react-dom-factories");

const {
  MANIFEST_ISSUE_LEVELS,
} = require("devtools/client/application/src/constants");
const Types = require("devtools/client/application/src/types/index");

const ManifestIssue = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestIssue")
);

/**
 * A collection of manifest issues (errors, warnings)
 */
class ManifestIssueList extends PureComponent {
  static get propTypes() {
    return {
      issues: Types.manifestIssueArray.isRequired,
    };
  }

  // group the errors by level, and order by severity
  groupIssuesByLevel() {
    const { issues } = this.props;

    const errors = issues.filter(x => x.level === MANIFEST_ISSUE_LEVELS.ERROR);
    const warnings = issues.filter(
      x => x.level === MANIFEST_ISSUE_LEVELS.WARNING
    );

    return [errors, warnings];
  }

  render() {
    const groups = this.groupIssuesByLevel().filter(list => !!list.length);

    return groups.map((list, listIndex) => {
      return ul(
        {
          className: "manifest-issues js-manifest-issues",
          key: `issuelist-${listIndex}`,
        },
        list.map((issue, issueIndex) =>
          ManifestIssue({
            className: "manifest-issues__item",
            key: `issue-${issueIndex}`,
            ...issue,
          })
        )
      );
    });
  }
}

// Exports
module.exports = ManifestIssueList;
