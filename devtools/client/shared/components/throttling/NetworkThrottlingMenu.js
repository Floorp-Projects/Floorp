/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const throttlingProfiles = require("./profiles");
const Types = require("./types");

// Localization
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/network-throttling.properties");
const NO_THROTTLING_LABEL = L10N.getStr("responsive.noThrottling");

loader.lazyRequireGetter(this, "showMenu", "devtools/client/shared/components/menu/utils", true);

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
    const {
      networkThrottling,
      onChangeNetworkThrottling,
    } = this.props;

    const menuItems = throttlingProfiles.map(profile => {
      return {
        label: profile.id,
        type: "checkbox",
        checked: networkThrottling.enabled && profile.id == networkThrottling.profile,
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
    const selectedProfile = networkThrottling.enabled ?
      networkThrottling.profile : NO_THROTTLING_LABEL;

    return (
      dom.button(
        {
          id: "network-throttling-menu",
          className: "devtools-button devtools-dropdown-button",
          title: selectedProfile,
          onClick: this.onShowThrottlingMenu,
        },
        dom.span({ className: "title" }, selectedProfile)
      )
    );
  }
}

module.exports = NetworkThrottlingMenu;
