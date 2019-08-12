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
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");

const { hr } = require("devtools/client/shared/vendor/react-dom-factories");

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

const { A11Y_LEARN_MORE_LINK } = require("../constants");
const { openDocLink } = require("devtools/client/shared/link");

const { updatePref } = require("../actions/ui");

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { PREFS } = require("../constants");

class AccessibilityTreeFilter extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      [PREFS.SCROLL_INTO_VIEW]: PropTypes.bool.isRequired,
    };
  }

  togglePref(prefKey) {
    this.props.dispatch(updatePref(prefKey, !this.props[prefKey]));
  }

  onPrefClick(prefKey) {
    this.togglePref(prefKey);
  }

  onLearnMoreClick() {
    openDocLink(
      A11Y_LEARN_MORE_LINK +
        "?utm_source=devtools&utm_medium=a11y-panel-toolbar"
    );
  }

  render() {
    return MenuButton(
      {
        menuId: "accessibility-tree-filters-prefs-menu",
        doc: document,
        className: `devtools-button badge toolbar-menu-button prefs`,
        title: L10N.getStr("accessibility.tree.filters.prefs"),
      },
      MenuList({}, [
        MenuItem({
          key: "pref-scroll-into-view",
          checked: this.props[PREFS.SCROLL_INTO_VIEW],
          className: `pref ${PREFS.SCROLL_INTO_VIEW}`,
          label: L10N.getStr("accessibility.pref.scroll.into.view.label"),
          tooltip: L10N.getStr("accessibility.pref.scroll.into.view.title"),
          onClick: this.onPrefClick.bind(this, PREFS.SCROLL_INTO_VIEW),
        }),
        hr(),
        MenuItem({
          role: "link",
          key: "accessibility-tree-filters-prefs-menu-help",
          className: "help",
          label: L10N.getStr("accessibility.documentation.label"),
          tooltip: L10N.getStr("accessibility.learnMore"),
          onClick: this.onLearnMoreClick,
        }),
      ])
    );
  }
}

const mapStateToProps = ({ ui }) => ({
  [PREFS.SCROLL_INTO_VIEW]: ui[PREFS.SCROLL_INTO_VIEW],
});

// Exports from this module
module.exports = connect(mapStateToProps)(AccessibilityTreeFilter);
