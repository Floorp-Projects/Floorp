/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const TreeView = createFactory(require("devtools/client/shared/components/tree/tree-view"));
const { L10N } = require("../../l10n");
const { getUrlHost } = require("../../request-utils");
const { getSelectedRequest } = require("../../selectors/index");

const { div, input } = DOM;

/*
 * Security panel component
 * If the site is being served over HTTPS, you get an extra tab labeled "Security".
 * This contains details about the secure connection used including the protocol,
 * the cipher suite, and certificate details
 */
function SecurityPanel({
  securityInfo,
  url,
}) {
  if (!securityInfo || !url) {
    return div();
  }

  const notAvailable = L10N.getStr("netmonitor.security.notAvailable");
  let object;

  if ((securityInfo.state === "secure" || securityInfo.state === "weak")) {
    const { subject, issuer, validity, fingerprint } = securityInfo.cert;
    const enabledLabel = L10N.getStr("netmonitor.security.enabled");
    const disabledLabel = L10N.getStr("netmonitor.security.disabled");

    object = {
      [L10N.getStr("netmonitor.security.connection")]: {
        [L10N.getStr("netmonitor.security.protocolVersion")]:
          securityInfo.protocolVersion || notAvailable,
        [L10N.getStr("netmonitor.security.cipherSuite")]:
          securityInfo.cipherSuite || notAvailable,
      },
      [L10N.getFormatStr("netmonitor.security.hostHeader", getUrlHost(url))]: {
        [L10N.getStr("netmonitor.security.hsts")]:
          securityInfo.hsts ? enabledLabel : disabledLabel,
        [L10N.getStr("netmonitor.security.hpkp")]:
          securityInfo.hpkp ? enabledLabel : disabledLabel,
      },
      [L10N.getStr("netmonitor.security.certificate")]: {
        [L10N.getStr("certmgr.subjectinfo.label")]: {
          [L10N.getStr("certmgr.certdetail.cn")]:
            subject.commonName || notAvailable,
          [L10N.getStr("certmgr.certdetail.o")]:
            subject.organization || notAvailable,
          [L10N.getStr("certmgr.certdetail.ou")]:
            subject.organizationUnit || notAvailable,
        },
        [L10N.getStr("certmgr.issuerinfo.label")]: {
          [L10N.getStr("certmgr.certdetail.cn")]:
            issuer.commonName || notAvailable,
          [L10N.getStr("certmgr.certdetail.o")]:
            issuer.organization || notAvailable,
          [L10N.getStr("certmgr.certdetail.ou")]:
            issuer.organizationUnit || notAvailable,
        },
        [L10N.getStr("certmgr.periodofvalidity.label")]: {
          [L10N.getStr("certmgr.begins")]:
            validity.start || notAvailable,
          [L10N.getStr("certmgr.expires")]:
            validity.end || notAvailable,
        },
        [L10N.getStr("certmgr.fingerprints.label")]: {
          [L10N.getStr("certmgr.certdetail.sha256fingerprint")]:
            fingerprint.sha256 || notAvailable,
          [L10N.getStr("certmgr.certdetail.sha1fingerprint")]:
            fingerprint.sha1 || notAvailable,
        },
      },
    };
  } else {
    object = {
      [L10N.getStr("netmonitor.security.error")]:
        new DOMParser().parseFromString(securityInfo.errorMessage, "text/html")
          .body.textContent || notAvailable
    };
  }

  return div({ id: "security-information" },
    TreeView({
      object,
      columns: [{
        id: "value",
        width: "100%",
      }],
      renderValue: renderValue.bind(null, securityInfo.weaknessReasons),
      expandedNodes: getExpandedNodes(object),
      expandableStrings: false,
    })
  );
}

SecurityPanel.displayName = "SecurityPanel";

SecurityPanel.propTypes = {
  securityInfo: PropTypes.object,
  url: PropTypes.string,
};

function renderValue(weaknessReasons = [], props) {
  const { member, value } = props;

  // Hide object summary
  if (typeof member.value === "object") {
    return null;
  }

  return div({ className: "security-info-value" },
    member.name === L10N.getStr("netmonitor.security.error") ?
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
    weaknessReasons.indexOf("cipher") !== -1 &&
    member.name === L10N.getStr("netmonitor.security.cipherSuite") ?
      // Display an extra warning icon after the cipher suite
      div({
        id: "security-warning-cipher",
        className: "security-warning-icon",
        title: L10N.getStr("netmonitor.security.warning.cipher"),
      })
      :
      null
  );
}

function getExpandedNodes(object, path = "", level = 0) {
  if (typeof object !== "object") {
    return null;
  }

  let expandedNodes = new Set();
  for (let prop in object) {
    let nodePath = path + "/" + prop;
    expandedNodes.add(nodePath);

    let nodes = getExpandedNodes(object[prop], nodePath, level + 1);
    if (nodes) {
      expandedNodes = new Set([...expandedNodes, ...nodes]);
    }
  }
  return expandedNodes;
}

module.exports = connect(
  (state) => {
    const selectedRequest = getSelectedRequest(state);

    if (selectedRequest) {
      const { securityInfo, url} = selectedRequest;
      return {
        securityInfo,
        url,
      };
    }

    return {};
  }
)(SecurityPanel);
