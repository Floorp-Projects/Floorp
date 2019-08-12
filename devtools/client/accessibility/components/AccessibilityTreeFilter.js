/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry */

// React
const {
  createFactory,
  Component,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  hr,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");

loader.lazyGetter(this, "MenuButton", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuButton")
  );
});
loader.lazyGetter(this, "MenuItem", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuItem")
  );
});
loader.lazyGetter(this, "MenuList", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuList")
  );
});

const actions = require("../actions/audit");

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { FILTERS } = require("../constants");

const TELEMETRY_AUDIT_ACTIVATED = "devtools.accessibility.audit_activated";
const FILTER_LABELS = {
  [FILTERS.NONE]: "accessibility.filter.none",
  [FILTERS.ALL]: "accessibility.filter.all2",
  [FILTERS.CONTRAST]: "accessibility.filter.contrast",
  [FILTERS.TEXT_LABEL]: "accessibility.filter.textLabel",
};

class AccessibilityTreeFilter extends Component {
  static get propTypes() {
    return {
      auditing: PropTypes.array.isRequired,
      filters: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      accessibilityWalker: PropTypes.object.isRequired,
      describedby: PropTypes.string,
    };
  }

  async toggleFilter(filterKey) {
    const { dispatch, filters, accessibilityWalker } = this.props;

    if (filterKey !== FILTERS.NONE && !filters[filterKey]) {
      if (gTelemetry) {
        gTelemetry.keyedScalarAdd(TELEMETRY_AUDIT_ACTIVATED, filterKey, 1);
      }

      dispatch(actions.auditing(filterKey));
      await dispatch(actions.audit(accessibilityWalker, filterKey));
    }

    // We wait to dispatch filter toggle until the tree is ready to be filtered
    // right after the audit. This is to make sure that we render an empty tree
    // (filtered) while the audit is running.
    dispatch(actions.filterToggle(filterKey));
  }

  onClick(filterKey) {
    this.toggleFilter(filterKey);
  }

  render() {
    const { auditing, filters, describedby } = this.props;
    const toolbarLabelID = "accessibility-tree-filters-label";
    const filterNoneChecked = !Object.values(filters).includes(true);
    const items = [
      MenuItem({
        key: FILTERS.NONE,
        checked: filterNoneChecked,
        className: `filter ${FILTERS.NONE}`,
        label: L10N.getStr(FILTER_LABELS[FILTERS.NONE]),
        onClick: this.onClick.bind(this, FILTERS.NONE),
        disabled: auditing.length > 0,
      }),
      hr(),
    ];

    const { [FILTERS.ALL]: filterAllChecked, ...filtersWithoutAll } = filters;
    items.push(
      MenuItem({
        key: FILTERS.ALL,
        checked: filterAllChecked,
        className: `filter ${FILTERS.ALL}`,
        label: L10N.getStr(FILTER_LABELS[FILTERS.ALL]),
        onClick: this.onClick.bind(this, FILTERS.ALL),
        disabled: auditing.length > 0,
      }),
      hr(),
      Object.entries(filtersWithoutAll).map(([filterKey, active]) =>
        MenuItem({
          key: filterKey,
          checked: active,
          className: `filter ${filterKey}`,
          label: L10N.getStr(FILTER_LABELS[filterKey]),
          onClick: this.onClick.bind(this, filterKey),
          disabled: auditing.length > 0,
        })
      )
    );

    let label;
    if (filterNoneChecked) {
      label = L10N.getStr(FILTER_LABELS[FILTERS.NONE]);
    } else if (filterAllChecked) {
      label = L10N.getStr(FILTER_LABELS[FILTERS.ALL]);
    } else {
      label = Object.keys(filtersWithoutAll)
        .filter(filterKey => filtersWithoutAll[filterKey])
        .map(filterKey => L10N.getStr(FILTER_LABELS[filterKey]))
        .join(", ");
    }

    return div(
      {
        role: "group",
        className: "accessibility-tree-filters",
        "aria-labelledby": toolbarLabelID,
        "aria-describedby": describedby,
      },
      span(
        { id: toolbarLabelID, role: "presentation" },
        L10N.getStr("accessibility.tree.filters")
      ),
      MenuButton(
        {
          menuId: "accessibility-tree-filters-menu",
          doc: document,
          className: `devtools-button badge toolbar-menu-button filters`,
          label,
        },
        MenuList({}, items)
      )
    );
  }
}

const mapStateToProps = ({ audit: { filters, auditing } }) => {
  return { filters, auditing };
};

// Exports from this module
module.exports = connect(mapStateToProps)(AccessibilityTreeFilter);
