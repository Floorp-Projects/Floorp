# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corp.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Terry Hayes <thayes@netscape.com>
#   Florian QUEZE <f.qu@queze.net>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

var security = {
  // Display the server certificate (static)
  viewCert : function () {
    var cert = security._cert;
    viewCertHelper(window, cert);
  },

  _getSecurityInfo : function() {
    const nsIX509Cert = Components.interfaces.nsIX509Cert;
    const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
    const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
    const nsISSLStatusProvider = Components.interfaces.nsISSLStatusProvider;
    const nsISSLStatus = Components.interfaces.nsISSLStatus;

    // We don't have separate info for a frame, return null until further notice
    // (see bug 138479)
    if (gWindow != gWindow.top)
      return null;

    var hName = null;
    try {
      hName = gWindow.location.host;
    }
    catch (exception) { }

    var ui = security._getSecurityUI();
    var status = null;
    var sp = null;
    var isBroken = false;
    if (ui) {
      isBroken = (ui.state == Components.interfaces.nsIWebProgressListener.STATE_IS_BROKEN);
      sp = ui.QueryInterface(nsISSLStatusProvider);
      if (sp)
        status = sp.SSLStatus;
    }
    if (status) {
      status = status.QueryInterface(nsISSLStatus);
    }
    if (status) {
      var cert = status.serverCert;
      var issuerName;

      issuerName = this.mapIssuerOrganization(cert.issuerOrganization);
      if (!issuerName) issuerName = cert.issuerName;

      return {
        hostName : hName,
        cAName : issuerName,
        encryptionAlgorithm : status.cipherName,
        encryptionStrength : status.secretKeyLength,
        isBroken : isBroken,
        cert : cert
      };
    } else {
      return {
        hostName : hName,
        cAName : "",
        encryptionAlgorithm : "",
        encryptionStrength : 0,
        isBroken : isBroken,
        cert : null
      };
    }
  },

  // Find the secureBrowserUI object (if present)
  _getSecurityUI : function() {
    if ("gBrowser" in window.opener)
      return window.opener.gBrowser.securityUI;
    return null;
  },

  // Interface for mapping a certificate issuer organization to
  // the value to be displayed.
  // Bug 82017 - this implementation should be moved to pipnss C++ code
  mapIssuerOrganization: function(name) {
    if (!name) return null;

    if (name == "RSA Data Security, Inc.") return "Verisign, Inc.";

    // No mapping required
    return name;
  },

  _cert : null
};

function securityOnLoad() {
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

  var info = security._getSecurityInfo();
  if (!info) {
    document.getElementById("securityTab").setAttribute("hidden", true);
    document.getElementById("securityBox").collapsed = true;
    return;
  }

  var idHdr;
  var message1;
  var message2;

  /* Set the identification messages */
  if (info.cert) {
    idHdr = bundle.GetStringFromName("pageInfo_WebSiteVerified");

    message1 = bundle.formatStringFromName("pageInfo_Identity_Verified",
                            [ info.hostName, info.cAName ],
                            2);
    setText("security-identity-text", message1);

    var viewText = bundle.GetStringFromName("pageInfo_ViewCertificate");
    setText("security-view-text", viewText);
    security._cert = info.cert;
  }
  else {
    idHdr = bundle.GetStringFromName("pageInfo_SiteNotVerified");
    var viewCert = document.getElementById("security-view-cert");
    viewCert.collapsed = true;
  }
  setText("general-security-identity", idHdr);
  setText("security-identity", idHdr);

  var hdr;
  var msg1;
  var msg2;

  /* Set the encryption messages */
  if (info.isBroken) {
    hdr = bundle.GetStringFromName("pageInfo_MixedContent");
    msg1 = bundle.GetStringFromName("pageInfo_Privacy_Mixed1");
    msg2 = bundle.GetStringFromName("pageInfo_Privacy_None2");
  }
  else if (info.encryptionStrength >= 90) {
    hdr = bundle.formatStringFromName("pageInfo_StrongEncryption",
                          [ info.encryptionAlgorithm, info.encryptionStrength + "" ], 2);
    msg1 = bundle.GetStringFromName("pageInfo_Privacy_Strong1");
    msg2 = bundle.GetStringFromName("pageInfo_Privacy_Strong2");
    security._cert = info.cert;
  }
  else if (info.encryptionStrength > 0) {
    hdr  = bundle.formatStringFromName("pageInfo_WeakEncryption",
                          [ info.encryptionAlgorithm, info.encryptionStrength + "" ], 2);
    msg1 = bundle.formatStringFromName("pageInfo_Privacy_Weak1", [ info.hostName ], 1);
    msg2 = bundle.GetStringFromName("pageInfo_Privacy_Weak2");
  }
  else {
    hdr = bundle.GetStringFromName("pageInfo_NoEncryption");
    if (info.hostName != null)
      msg1 = bundle.formatStringFromName("pageInfo_Privacy_None1", [ info.hostName ], 1);
    else
      msg1 = bundle.GetStringFromName("pageInfo_Privacy_None3");
    msg2 = bundle.GetStringFromName("pageInfo_Privacy_None2");
  }
  setText("general-security-privacy", hdr);
  setText("security-privacy", hdr);
  setText("security-privacy-msg1", msg1);
  setText("security-privacy-msg2", msg2);
}

function setText(id, value)
{
  var element = document.getElementById(id);
  if (!element)
    return;
  if (element.localName == "textbox")
    element.value = value;
  else {
    if (element.hasChildNodes())
      element.removeChild(element.firstChild);
    var textNode = document.createTextNode(value);
    element.appendChild(textNode);
  }
}

function viewCertHelper(parent, cert)
{
  if (!cert)
    return;

  var cd = Components.classes[CERTIFICATEDIALOGS_CONTRACTID].getService(nsICertificateDialogs);
  cd.viewCert(parent, cert);
}
