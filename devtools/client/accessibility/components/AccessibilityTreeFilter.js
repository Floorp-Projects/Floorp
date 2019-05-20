/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry */

// React
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const { div, span } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const ToggleButton = createFactory(require("./Button").ToggleButton);

const actions = require("../actions/audit");

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { FILTERS } = require("../constants");

const TELEMETRY_AUDIT_ACTIVATED = "devtools.accessibility.audit_activated";
const FILTER_LABELS = {
  [FILTERS.CONTRAST]: "accessibility.badge.contrast",
};

class AccessibilityTreeFilter extends Component {
  static get propTypes() {
    return {
      auditing: PropTypes.string.isRequired,
      filters: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      walker: PropTypes.object.isRequired,
      describedby: PropTypes.string,
    };
  }

  async toggleFilter(filterKey) {
    const { dispatch, filters, walker } = this.props;

    if (!filters[filterKey]) {
      if (gTelemetry) {
        gTelemetry.keyedScalarAdd(TELEMETRY_AUDIT_ACTIVATED, filterKey, 1);
      }

      dispatch(actions.auditing(filterKey));
      await dispatch(actions.audit(walker, filterKey));
    }

    // We wait to dispatch filter toggle until the tree is ready to be filtered
    // right after the audit. This is to make sure that we render an empty tree
    // (filtered) while the audit is running.
    dispatch(actions.filterToggle(filterKey));
  }

  onClick(filterKey, e) {
    const { mozInputSource, MOZ_SOURCE_KEYBOARD } = e.nativeEvent;
    if (e.isTrusted && mozInputSource === MOZ_SOURCE_KEYBOARD) {
      // Already handled by key down handler on user input.
      return;
    }

    this.toggleFilter(filterKey);
  }

  onKeyDown(filterKey, e) {
    // We explicitely handle "click" and "keydown" events this way here because
    // of the expectation of both Space and Enter triggering the click event
    // even though Enter is the only one in the spec.
    if (![" ", "Enter"].includes(e.key)) {
      return;
    }

    this.toggleFilter(filterKey);
  }

  render() {
    const { auditing, filters, describedby } = this.props;
    const toolbarLabelID = "accessibility-tree-filters-label";
    const filterButtons = Object.entries(filters).map(([filterKey, active]) =>
      ToggleButton({
        className: "badge",
        key: filterKey,
        active,
        label: L10N.getStr(FILTER_LABELS[filterKey]),
        onClick: this.onClick.bind(this, filterKey),
        onKeyDown: this.onKeyDown.bind(this, filterKey),
        busy: auditing === filterKey,
      }));

    return div({
      role: "toolbar",
      className: "accessibility-tree-filters",
      "aria-labelledby": toolbarLabelID,
      "aria-describedby": describedby,
    },
      span({ id: toolbarLabelID, role: "presentation" },
        L10N.getStr("accessibility.tree.filters")),
      ...filterButtons);
  }
}

const mapStateToProps = ({ audit: { filters, auditing } }) => {
  return { filters, auditing };
};

// Exports from this module
module.exports = connect(mapStateToProps)(AccessibilityTreeFilter);
