/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");

class IssueItem extends PureComponent {
  static get propTypes() {
    return {
      ...Types.issue,
    };
  }

  _isTesting() {
    return Services.prefs.getBoolPref("devtools.testing", false);
  }

  render() {
    const { property, type } = this.props;

    const qaDatasetForLi = this._isTesting()
      ? { "data-qa-property": property }
      : {};

    return dom.li(
      {
        key: `${property}:${type}`,
        ...qaDatasetForLi,
      },
      Object.entries(this.props).map(([key, value]) => {
        const qaDatasetForField = this._isTesting()
          ? {
              "data-qa-key": key,
              "data-qa-value": JSON.stringify(value),
            }
          : {};

        return dom.div(
          {
            key,
            ...qaDatasetForField,
          },
          `${key}:${JSON.stringify(value)}`
        );
      })
    );
  }
}

module.exports = IssueItem;
