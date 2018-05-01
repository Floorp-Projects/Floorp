/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");
const { getStr } = require("../utils/l10n");
const throttlingProfiles = require("devtools/client/shared/network-throttling-profiles");

class NetworkThrottlingSelector extends PureComponent {
  static get propTypes() {
    return {
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      onChangeNetworkThrottling: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onSelectChange = this.onSelectChange.bind(this);
  }

  onSelectChange({ target }) {
    let {
      onChangeNetworkThrottling,
    } = this.props;

    if (target.value == getStr("responsive.noThrottling")) {
      onChangeNetworkThrottling(false, "");
      return;
    }

    for (let profile of throttlingProfiles) {
      if (profile.id === target.value) {
        onChangeNetworkThrottling(true, profile.id);
        return;
      }
    }
  }

  render() {
    let {
      networkThrottling,
    } = this.props;

    let selectClass = "toolbar-dropdown";
    let selectedProfile;
    if (networkThrottling.enabled) {
      selectClass += " selected";
      selectedProfile = networkThrottling.profile;
    } else {
      selectedProfile = getStr("responsive.noThrottling");
    }

    let listContent = [
      dom.option(
        {
          key: "disabled",
        },
        getStr("responsive.noThrottling")
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
