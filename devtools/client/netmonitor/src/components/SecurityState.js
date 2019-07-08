/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const { propertiesEqual } = require("../utils/request-utils");

const UPDATED_DOMAIN_PROPS = ["remoteAddress", "securityState", "urlDetails"];

class SecurityState extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(
      UPDATED_DOMAIN_PROPS,
      this.props.item,
      nextProps.item
    );
  }

  render() {
    const { item, onSecurityIconMouseDown } = this.props;

    const {
      securityState,
      urlDetails: { isLocal },
    } = item;
    const iconClassList = ["requests-security-state-icon"];

    let iconTitle;
    let realSecurityState = securityState;

    // Locally delivered files such as http://localhost and file:// paths
    // are considered to have been delivered securely.
    if (isLocal) {
      realSecurityState = "secure";
    }

    if (realSecurityState) {
      iconClassList.push(`security-state-${realSecurityState}`);
      iconTitle = L10N.getStr(`netmonitor.security.state.${realSecurityState}`);
    }

    return div(
      {},
      div({
        className: iconClassList.join(" "),
        onMouseDown: onSecurityIconMouseDown,
        title: iconTitle,
      }),
      item.isThirdPartyTrackingResource &&
        div({
          className: "tracking-resource",
          title: L10N.getStr("netmonitor.trackingResource.tooltip"),
        })
    );
  }
}

module.exports = SecurityState;
