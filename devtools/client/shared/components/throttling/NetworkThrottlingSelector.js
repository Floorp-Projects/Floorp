/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("./types");
const throttlingProfiles = require("./profiles");

// Localization
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/network-throttling.properties");

/**
 * This component represents selector button that can be used
 * to throttle network bandwidth.
 */
class NetworkThrottlingSelector extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      onChangeNetworkThrottling: PropTypes.func.isRequired,
    };
  }

  static get defaultProps() {
    return {
      className: "",
    };
  }

  constructor(props) {
    super(props);
    this.onSelectChange = this.onSelectChange.bind(this);
  }

  onSelectChange({ target }) {
    const {
      onChangeNetworkThrottling,
    } = this.props;

    if (target.value == L10N.getStr("responsive.noThrottling")) {
      onChangeNetworkThrottling(false, "");
      return;
    }

    for (const profile of throttlingProfiles) {
      if (profile.id === target.value) {
        onChangeNetworkThrottling(true, profile.id);
        return;
      }
    }
  }

  render() {
    const {
      className,
      networkThrottling,
    } = this.props;

    let selectClass = className + " toolbar-dropdown";
    let selectedProfile;
    if (networkThrottling.enabled) {
      selectClass += " selected";
      selectedProfile = networkThrottling.profile;
    } else {
      selectedProfile = L10N.getStr("responsive.noThrottling");
    }

    const listContent = [
      dom.option(
        {
          key: "disabled",
        },
        L10N.getStr("responsive.noThrottling")
      ),
      dom.option(
        {
          key: "divider",
          className: "divider",
          disabled: true,
        }
      ),
      throttlingProfiles.map(profile => {
        return dom.option(
          {
            key: profile.id,
          },
          profile.id
        );
      }),
    ];

    return dom.select(
      {
        id: "global-network-throttling-selector",
        className: selectClass,
        value: selectedProfile,
        onChange: this.onSelectChange,
      },
      ...listContent
    );
  }
}

module.exports = NetworkThrottlingSelector;
