/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  td,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getFormattedIPAndPort,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");
const {
  propertiesEqual,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");
const SecurityState = createFactory(
  require("resource://devtools/client/netmonitor/src/components/SecurityState.js")
);

const UPDATED_DOMAIN_PROPS = ["remoteAddress", "securityState", "urlDetails"];

class RequestListColumnDomain extends Component {
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
      remoteAddress,
      remotePort,
      urlDetails: { host, isLocal },
    } = item;

    const title =
      host +
      (remoteAddress
        ? ` (${getFormattedIPAndPort(remoteAddress, remotePort)})`
        : "");

    return td(
      { className: "requests-list-column requests-list-domain", title },
      SecurityState({ item, onSecurityIconMouseDown, isLocal }),
      host
    );
  }
}

module.exports = RequestListColumnDomain;
