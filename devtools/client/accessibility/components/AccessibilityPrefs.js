/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  createFactory,
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");

const {
  hr,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

loader.lazyGetter(this, "MenuButton", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuButton.js")
  );
});
loader.lazyGetter(this, "MenuItem", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuItem.js")
  );
});
loader.lazyGetter(this, "MenuList", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuList.js")
  );
});

const {
  A11Y_LEARN_MORE_LINK,
} = require("resource://devtools/client/accessibility/constants.js");
const { openDocLink } = require("resource://devtools/client/shared/link.js");

const {
  updatePref,
} = require("resource://devtools/client/accessibility/actions/ui.js");

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  PREFS,
} = require("resource://devtools/client/accessibility/constants.js");

class AccessibilityPrefs extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      [PREFS.SCROLL_INTO_VIEW]: PropTypes.bool.isRequired,
      toolboxDoc: PropTypes.object.isRequired,
    };
  }

  togglePref(prefKey) {
    this.props.dispatch(updatePref(prefKey, !this.props[prefKey]));
  }

  onPrefClick(prefKey) {
    this.togglePref(prefKey);
  }

  onLearnMoreClick() {
    openDocLink(A11Y_LEARN_MORE_LINK);
  }

  render() {
    return MenuButton(
      {
        menuId: "accessibility-tree-filters-prefs-menu",
        toolboxDoc: this.props.toolboxDoc,
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
        hr({ key: "hr" }),
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
module.exports = connect(mapStateToProps)(AccessibilityPrefs);
