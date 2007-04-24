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
        cert : cert,
        fullLocation : gWindow.location
      };
    } else {
      return {
        hostName : hName,
        cAName : "",
        encryptionAlgorithm : "",
        encryptionStrength : 0,
        isBroken : isBroken,
        cert : null,
        fullLocation : gWindow.location        
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
  
  /**
   * Open the cookie manager window
   */
  viewCookies : function()
  {
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    var win = wm.getMostRecentWindow("Browser:Cookies");
    if (win)
      win.focus();
    else
      window.openDialog("chrome://browser/content/preferences/cookies.xul",
                        "Browser:Cookies", "");
  },
  
  /**
   * Open the password manager window
   */
  viewPasswords : function()
  {
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    var win = wm.getMostRecentWindow("Toolkit:PasswordManager");
    if (win)
      win.focus();
    else
      window.openDialog("chrome://passwordmgr/content/passwordManager.xul",
                        "Toolkit:PasswordManager", "");
  },

  _cert : null
};

function securityOnLoad() {
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var pageInfoBundle = document.getElementById("pageinfobundle");

  var info = security._getSecurityInfo();
  if (!info) {
    document.getElementById("securityTab").setAttribute("hidden", true);
    document.getElementById("securityBox").collapsed = true;
    return;
  }

  /* Set Identity section text */
  setText("security-identity-domain-value", info.hostName);
  
  // FIXME - Should only be showing the next two if the cert is EV.  Waiting on
  // bug 374336
  var owner, verifier, generalPageIdentityString;
  if (info.cert && !info.isBroken) {
    // Try to pull out meaningful values.  Technically these fields are optional
    // so we'll employ fallbacks where appropriate.  The EV spec states that Org
    // fields must be specified for subject and issuer so when 374336 lands, this
    // code can be simplified.
    owner = info.cert.organization || info.cert.commonName ||
            info.cert.subjectName;
    verifier = security.mapIssuerOrganization(info.cAName ||
                                              info.cert.issuerCommonName ||
                                              info.cert.issuerName);
    generalPageIdentityString = pageInfoBundle.getFormattedString("generalSiteIdentity",
                                                                  [owner, verifier]);
  }
  else {
    // We don't have valid identity credentials.
    owner = pageInfoBundle.getString("securityNoIdentity");
    verifier = pageInfoBundle.getString("notset");
    generalPageIdentityString = owner;
  }

  setText("security-identity-owner-value", owner);
  setText("security-identity-verifier-value", verifier);
  setText("general-security-identity", generalPageIdentityString);

  /* Manage the View Cert button*/
  if (info.cert) {
    var viewText = bundle.GetStringFromName("pageInfo_ViewCertificate");
    setText("security-view-text", viewText);
    security._cert = info.cert;
  }
  else {
    var viewCert = document.getElementById("security-view-cert");
    viewCert.collapsed = true;
  }

  /* Set Privacy & History section text */
  var yesStr = pageInfoBundle.getString("yes");
  var noStr = pageInfoBundle.getString("no");

  setText("security-privacy-cookies-value",
          hostHasCookies(info.hostName) ? yesStr : noStr);
  setText("security-privacy-passwords-value",
          realmHasPasswords(info.fullLocation) ? yesStr : noStr);
  
  var visitCount = previousVisitCount(info.hostName);
  if(visitCount > 1) {
    setText("security-privacy-history-value",
            pageInfoBundle.getFormattedString("securityNVisits", [visitCount]));
  }
  else if (visitCount == 1) {
    setText("security-privacy-history-value",
            pageInfoBundle.getString("securityOneVisit"));
  }
  else {
    setText("security-privacy-history-value", noStr);        
  }

  /* Set the Technical Detail section messages */
  var hdr;
  var msg1;
  var msg2;

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
  setText("security-technical-shortform", hdr);
  setText("security-technical-longform1", msg1);
  setText("security-technical-longform2", msg2); 
  setText("general-security-privacy", hdr);
}

function setText(id, value)
{
  var element = document.getElementById(id);
  if (!element)
    return;
  if (element.localName == "textbox" || element.localName == "label")
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

/**
 * Return true iff we have cookies for hostName
 */
function hostHasCookies(hostName) {
  if (!hostName)
    return false;
  
  var cookieManager = Components.classes["@mozilla.org/cookiemanager;1"]
                                .getService(Components.interfaces.nsICookieManager);

  var iter = cookieManager.enumerator;
  while (iter.hasMoreElements()){
    var cookie = iter.getNext().QueryInterface(Components.interfaces.nsICookie);
    if (!cookie)
      continue;
    
    // A direct match works whether it's a domain cookie or not
    if (cookie.host == hostName)
      return true;
    
    // Domain cookies just need to end with our target hostname
    if (cookie.isDomain && endsWith(hostName, cookie.host))
      return true;
  }
  return false;
}

/**
 * Return true iff realm (proto://host:port) (extracted from location) has
 * saved passwords
 */
function realmHasPasswords(location) {
  if (!location) 
    return false;
  
  var realm = makeURI(location).prePath;
  var passwordManager = Components.classes["@mozilla.org/passwordmanager;1"]
                                  .getService(Components.interfaces.nsIPasswordManager);
  var e = passwordManager.enumerator;
  while (e.hasMoreElements()) {
    var next = e.getNext().QueryInterface(Components.interfaces.nsIPassword);
    if (!next)
      continue;
    
    if (realm == next.host)
      return true;
  }
  return false;
}

/**
 * Return the number of previous visits recorded for host before today.
 *
 * @param host - the domain name to look for in history
 */
function previousVisitCount(host, endTimeReference) {
  if (!host)
    return false;
  
  var historyService = Components.classes["@mozilla.org/browser/nav-history-service;1"]
                                 .getService(Components.interfaces.nsINavHistoryService);
    
  var options = historyService.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_VISIT;
  
  // Search for visits to this host before today
  var query = historyService.getNewQuery();
  query.endTimeReference = query.TIME_RELATIVE_TODAY;
  query.endTime = 0;
  query.domain = host;

  var result = historyService.executeQuery(query, options);
  result.root.containerOpen = true;
  return result.root.childCount;
}

/**
 * Return true iff the string suffix appears at the end of the string target
 */
function endsWith(target, suffix) {
  return target && suffix && target.substr(-1 * suffix.length) === suffix;
}
