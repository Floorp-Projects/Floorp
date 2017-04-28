/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getFormattedIPAndPort } = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const { propertiesEqual } = require("../utils/request-utils");

const { div } = DOM;

const UPDATED_DOMAIN_PROPS = [
  "remoteAddress",
  "securityState",
  "urlDetails",
];

const RequestListColumnDomain = createClass({
  displayName: "RequestListColumnDomain",

  propTypes: {
    item: PropTypes.object.isRequired,
    onSecurityIconClick: PropTypes.func.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_DOMAIN_PROPS, this.props.item, nextProps.item);
  },

  render() {
    let { item, onSecurityIconClick } = this.props;
    let { remoteAddress, remotePort, securityState,
      urlDetails: { host, isLocal } } = item;
    let iconClassList = ["requests-security-state-icon"];
    let iconTitle;
    let title = host + (remoteAddress ?
      ` (${getFormattedIPAndPort(remoteAddress, remotePort)})` : "");

    if (isLocal) {
      iconClassList.push("security-state-local");
      iconTitle = L10N.getStr("netmonitor.security.state.secure");
    } else if (securityState) {
      iconClassList.push(`security-state-${securityState}`);
      iconTitle = L10N.getStr(`netmonitor.security.state.${securityState}`);
    }

    return (
      div({ className: "requests-list-column requests-list-domain", title },
        div({
          className: iconClassList.join(" "),
          onMouseDown: onSecurityIconClick,
          title: iconTitle,
        }),
        host,
      )
    );
  }
});

module.exports = RequestListColumnDomain;
