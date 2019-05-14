/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { PluralForm } = require("devtools/shared/plural-form");

const { L10N } = require("../utils/l10n");

/**
 * Helper functional component to render an accessible text progressbar.
 * @param {Object} props
 *        - id for the progressbar element
 *        - valuetext for the progressbar element
 */
function TextProgressBar({ id, textStringKey }) {
  const text = L10N.getStr(textStringKey);
  return ReactDOM.span({
    id,
    key: id,
    role: "progressbar",
    "aria-valuetext": text,
  },
    text);
}

class AuditProgressOverlay extends React.Component {
  static get propTypes() {
    return {
      auditing: PropTypes.array,
      total: PropTypes.number,
      percentage: PropTypes.number,
    };
  }

  render() {
    const { auditing, percentage, total } = this.props;
    if (!auditing) {
      return null;
    }

    const id = "audit-progress-container";

    if (total == null) {
      return TextProgressBar({id, textStringKey: "accessibility.progress.initializing"});
    }

    if (percentage === 100) {
      return TextProgressBar({id, textStringKey: "accessibility.progress.finishing"});
    }

    const progressbarString = PluralForm.get(total,
      L10N.getStr("accessibility.progress.progressbar"));

    return ReactDOM.span({
      id,
      key: id,
    },
      progressbarString.replace("#1", total),
      ReactDOM.progress({
        max: 100,
        value: percentage,
        className: "audit-progress-progressbar",
        "aria-labelledby": id,
      }));
  }
}

const mapStateToProps = ({ audit: { auditing, progress }}) => {
  const { total, percentage } = progress || {};
  return { auditing, total, percentage };
};

module.exports = connect(mapStateToProps)(AuditProgressOverlay);
