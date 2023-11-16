/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const throttlingProfiles = require("resource://devtools/client/shared/components/throttling/profiles.js");
const Types = require("resource://devtools/client/shared/components/throttling/types.js");

// Localization
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/network-throttling.properties"
);
const NO_THROTTLING_LABEL = L10N.getStr("responsive.noThrottling");

loader.lazyRequireGetter(
  this,
  "showMenu",
  "resource://devtools/client/shared/components/menu/utils.js",
  true
);

/**
 * This component represents selector button that can be used
 * to throttle network bandwidth.
 */
class NetworkThrottlingMenu extends PureComponent {
  static get propTypes() {
    return {
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      onChangeNetworkThrottling: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowThrottlingMenu = this.onShowThrottlingMenu.bind(this);
  }

  onShowThrottlingMenu(event) {
    const { networkThrottling, onChangeNetworkThrottling } = this.props;

    const menuItems = throttlingProfiles.profiles.map(profile => {
      return {
        label: profile.id,
        type: "checkbox",
        checked:
          networkThrottling.enabled && profile.id == networkThrottling.profile,
        click: () => onChangeNetworkThrottling(true, profile.id),
      };
    });

    menuItems.unshift("-");

    menuItems.unshift({
      label: NO_THROTTLING_LABEL,
      type: "checkbox",
      checked: !networkThrottling.enabled,
      click: () => onChangeNetworkThrottling(false, ""),
    });

    showMenu(menuItems, { button: event.target });
  }

  render() {
    const { networkThrottling } = this.props;
    const label = networkThrottling.enabled
      ? networkThrottling.profile
      : NO_THROTTLING_LABEL;

    let title = NO_THROTTLING_LABEL;

    if (networkThrottling.enabled) {
      const id = networkThrottling.profile;
      const selectedProfile = throttlingProfiles.profiles.find(
        profile => profile.id === id
      );
      title = selectedProfile.description;
    }

    return dom.button(
      {
        id: "network-throttling-menu",
        className: "devtools-button devtools-dropdown-button",
        title,
        onClick: this.onShowThrottlingMenu,
      },
      dom.span({ className: "title" }, label)
    );
  }
}

module.exports = NetworkThrottlingMenu;
