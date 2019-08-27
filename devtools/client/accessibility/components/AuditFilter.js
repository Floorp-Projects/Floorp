/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { isFiltered } = require("../utils/audit");
const { FILTERS } = require("../constants");
const {
  accessibility: {
    AUDIT_TYPE,
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
  },
} = require("devtools/shared/constants");

function validateCheck({ error, score }) {
  return !error && [BEST_PRACTICES, FAIL, WARNING].includes(score);
}

const AUDIT_TYPE_TO_FILTER = {
  [AUDIT_TYPE.CONTRAST]: {
    filterKey: FILTERS.CONTRAST,
    validator: validateCheck,
  },
  [AUDIT_TYPE.KEYBOARD]: {
    filterKey: FILTERS.KEYBOARD,
    validator: validateCheck,
  },
  [AUDIT_TYPE.TEXT_LABEL]: {
    filterKey: FILTERS.TEXT_LABEL,
    validator: validateCheck,
  },
};

class AuditFilter extends React.Component {
  static get propTypes() {
    return {
      checks: PropTypes.object,
      children: PropTypes.any,
      filters: PropTypes.object.isRequired,
    };
  }

  isVisible(filters) {
    return !isFiltered(filters);
  }

  shouldHide() {
    const { filters, checks } = this.props;
    if (this.isVisible(filters)) {
      return false;
    }

    if (!checks || Object.values(checks).every(check => check == null)) {
      return true;
    }

    for (const type in checks) {
      if (
        AUDIT_TYPE_TO_FILTER[type] &&
        checks[type] &&
        filters[AUDIT_TYPE_TO_FILTER[type].filterKey] &&
        AUDIT_TYPE_TO_FILTER[type].validator(checks[type])
      ) {
        return false;
      }
    }

    return true;
  }

  render() {
    return this.shouldHide() ? null : this.props.children;
  }
}

const mapStateToProps = ({ audit: { filters } }) => {
  return { filters };
};

module.exports = connect(mapStateToProps)(AuditFilter);
