/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { td } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFormattedIPAndPort } = require("../utils/format-utils");
const { propertiesEqual } = require("../utils/request-utils");
const SecurityState = createFactory(require("./SecurityState"));

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
