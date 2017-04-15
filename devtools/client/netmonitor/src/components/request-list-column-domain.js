/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils/l10n");
const { propertiesEqual } = require("../utils/request-utils");

const { div, span } = DOM;

const UPDATED_DOMAIN_PROPS = [
  "urlDetails",
  "remoteAddress",
  "securityState",
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
    const { item, onSecurityIconClick } = this.props;
    const { urlDetails, remoteAddress, securityState } = item;

    let iconClassList = ["requests-security-state-icon"];
    let iconTitle;
    if (urlDetails.isLocal) {
      iconClassList.push("security-state-local");
      iconTitle = L10N.getStr("netmonitor.security.state.secure");
    } else if (securityState) {
      iconClassList.push(`security-state-${securityState}`);
      iconTitle = L10N.getStr(`netmonitor.security.state.${securityState}`);
    }

    let title = urlDetails.host + (remoteAddress ? ` (${remoteAddress})` : "");

    return (
      div({ className: "requests-list-subitem requests-list-security-and-domain" },
        div({
          className: iconClassList.join(" "),
          title: iconTitle,
          onClick: onSecurityIconClick,
        }),
        span({ className: "subitem-label requests-list-domain", title }, urlDetails.host),
      )
    );
  }
});

module.exports = RequestListColumnDomain;
