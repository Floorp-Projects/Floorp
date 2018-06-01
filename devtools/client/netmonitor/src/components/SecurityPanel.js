/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");
const {
  fetchNetworkUpdatePacket,
  getUrlHost,
} = require("../utils/request-utils");

// Components
const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
const PropertiesView = createFactory(require("./PropertiesView"));

const { div, input, span } = dom;
const NOT_AVAILABLE = L10N.getStr("netmonitor.security.notAvailable");
const ERROR_LABEL = L10N.getStr("netmonitor.security.error");
const CIPHER_SUITE_LABEL = L10N.getStr("netmonitor.security.cipherSuite");
const WARNING_CIPHER_LABEL = L10N.getStr("netmonitor.security.warning.cipher");
const ENABLED_LABEL = L10N.getStr("netmonitor.security.enabled");
const DISABLED_LABEL = L10N.getStr("netmonitor.security.disabled");
const CONNECTION_LABEL = L10N.getStr("netmonitor.security.connection");
const PROTOCOL_VERSION_LABEL = L10N.getStr("netmonitor.security.protocolVersion");
const KEA_GROUP_LABEL = L10N.getStr("netmonitor.security.keaGroup");
const SIGNATURE_SCHEME_LABEL = L10N.getStr("netmonitor.security.signatureScheme");
const HSTS_LABEL = L10N.getStr("netmonitor.security.hsts");
const HPKP_LABEL = L10N.getStr("netmonitor.security.hpkp");
const CERTIFICATE_LABEL = L10N.getStr("netmonitor.security.certificate");
const CERTIFICATE_TRANSPARENCY_LABEL =
    L10N.getStr("certmgr.certificateTransparency.label");
const SUBJECT_INFO_LABEL = L10N.getStr("certmgr.subjectinfo.label");
const CERT_DETAIL_COMMON_NAME_LABEL = L10N.getStr("certmgr.certdetail.cn");
const CERT_DETAIL_ORG_LABEL = L10N.getStr("certmgr.certdetail.o");
const CERT_DETAIL_ORG_UNIT_LABEL = L10N.getStr("certmgr.certdetail.ou");
const ISSUER_INFO_LABEL = L10N.getStr("certmgr.issuerinfo.label");
const PERIOD_OF_VALIDITY_LABEL = L10N.getStr("certmgr.periodofvalidity.label");
const BEGINS_LABEL = L10N.getStr("certmgr.begins");
const EXPIRES_LABEL = L10N.getStr("certmgr.expires");
const FINGERPRINTS_LABEL = L10N.getStr("certmgr.fingerprints.label");
const SHA256_FINGERPRINT_LABEL =
  L10N.getStr("certmgr.certdetail.sha256fingerprint");
const SHA1_FINGERPRINT_LABEL = L10N.getStr("certmgr.certdetail.sha1fingerprint");

/*
 * Security panel component
 * If the site is being served over HTTPS, you get an extra tab labeled "Security".
 * This contains details about the secure connection used including the protocol,
 * the cipher suite, and certificate details
 */
class SecurityPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      request: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, ["securityInfo"]);
  }

  componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, ["securityInfo"]);
  }

  renderValue(props, weaknessReasons = []) {
    const { member, value } = props;

    // Hide object summary
    if (typeof member.value === "object") {
      return null;
    }

    return span({ className: "security-info-value" },
      member.name === ERROR_LABEL ?
        // Display multiline text for security error
        value
        :
        // Display one line selectable text for security details
        input({
          className: "textbox-input",
          readOnly: "true",
          value,
        })
      ,
      weaknessReasons.includes("cipher") &&
      member.name === CIPHER_SUITE_LABEL ?
        // Display an extra warning icon after the cipher suite
        div({
          id: "security-warning-cipher",
          className: "security-warning-icon",
          title: WARNING_CIPHER_LABEL,
        })
        :
        null
    );
  }

  render() {
    const { openLink, request } = this.props;
    const { securityInfo, url } = request;

    if (!securityInfo || !url) {
      return null;
    }

    let object;

    if (securityInfo.state === "secure" || securityInfo.state === "weak") {
      const { subject, issuer, validity, fingerprint } = securityInfo.cert;
      const HOST_HEADER_LABEL = L10N.getFormatStr("netmonitor.security.hostHeader",
        getUrlHost(url));

      object = {
        [CONNECTION_LABEL]: {
          [PROTOCOL_VERSION_LABEL]:
            securityInfo.protocolVersion || NOT_AVAILABLE,
          [CIPHER_SUITE_LABEL]:
            securityInfo.cipherSuite || NOT_AVAILABLE,
          [KEA_GROUP_LABEL]:
            securityInfo.keaGroupName || NOT_AVAILABLE,
          [SIGNATURE_SCHEME_LABEL]:
            securityInfo.signatureSchemeName || NOT_AVAILABLE,
        },
        [HOST_HEADER_LABEL]: {
          [HSTS_LABEL]:
            securityInfo.hsts ? ENABLED_LABEL : DISABLED_LABEL,
          [HPKP_LABEL]:
            securityInfo.hpkp ? ENABLED_LABEL : DISABLED_LABEL,
        },
        [CERTIFICATE_LABEL]: {
          [SUBJECT_INFO_LABEL]: {
            [CERT_DETAIL_COMMON_NAME_LABEL]:
              subject.commonName || NOT_AVAILABLE,
            [CERT_DETAIL_ORG_LABEL]:
              subject.organization || NOT_AVAILABLE,
            [CERT_DETAIL_ORG_UNIT_LABEL]:
              subject.organizationUnit || NOT_AVAILABLE,
          },
          [ISSUER_INFO_LABEL]: {
            [CERT_DETAIL_COMMON_NAME_LABEL]:
              issuer.commonName || NOT_AVAILABLE,
            [CERT_DETAIL_ORG_LABEL]:
              issuer.organization || NOT_AVAILABLE,
            [CERT_DETAIL_ORG_UNIT_LABEL]:
              issuer.organizationUnit || NOT_AVAILABLE,
          },
          [PERIOD_OF_VALIDITY_LABEL]: {
            [BEGINS_LABEL]:
              validity.start || NOT_AVAILABLE,
            [EXPIRES_LABEL]:
              validity.end || NOT_AVAILABLE,
          },
          [FINGERPRINTS_LABEL]: {
            [SHA256_FINGERPRINT_LABEL]:
              fingerprint.sha256 || NOT_AVAILABLE,
            [SHA1_FINGERPRINT_LABEL]:
              fingerprint.sha1 || NOT_AVAILABLE,
          },
          [CERTIFICATE_TRANSPARENCY_LABEL]:
          securityInfo.certificateTransparency || NOT_AVAILABLE,
        },
      };
    } else {
      object = {
        [ERROR_LABEL]: securityInfo.errorMessage || NOT_AVAILABLE
      };
    }

    return div({ className: "panel-container security-panel" },
      PropertiesView({
        object,
        renderValue: (props) => this.renderValue(props, securityInfo.weaknessReasons),
        enableFilter: false,
        expandedNodes: TreeViewClass.getExpandedNodes(object),
        openLink,
      })
    );
  }
}

module.exports = SecurityPanel;
