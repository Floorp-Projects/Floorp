/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* We expect the following arguments:
   - pref name of LDAP directory to fetch from
   - array with email addresses

  Display modal dialog with message and stop button.
  In onload, kick off binding to LDAP.
  When bound, kick off the searches.
  On finding certificates, import into permanent cert database.
  When all searches are finished, close the dialog.
*/

const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const CertAttribute = "usercertificate;binary";

var gEmailAddresses;
var gDirectoryPref;
var gLdapServerURL;
var gLdapConnection;
var gCertDB;
var gLdapOperation;

function onLoad()
{
  gDirectoryPref = window.arguments[0];
  gEmailAddresses = window.arguments[1];

  if (!gEmailAddresses.length)
  {
    window.close();
    return;
  }

  setTimeout(search, 1);
}

function search()
{
  var prefService =
    Components.classes["@mozilla.org/preferences-service;1"]
      .getService(Components.interfaces.nsIPrefService);
  var prefs = prefService.getBranch(null);

  gLdapServerURL =
    Components.classes["@mozilla.org/network/ldap-url;1"]
      .createInstance().QueryInterface(Components.interfaces.nsILDAPURL);

  try {
    gLdapServerURL.spec = prefs.getCharPref(gDirectoryPref + ".uri");

    gLdapConnection = Components.classes["@mozilla.org/network/ldap-connection;1"]
      .createInstance().QueryInterface(Components.interfaces.nsILDAPConnection);

    gLdapConnection.init(
      gLdapServerURL.asciiHost,
      gLdapServerURL.port,
      gLdapServerURL.options,
      null,
      getProxyOnUIThread(new boundListener(),
                            Components.interfaces.nsILDAPMessageListener),
      null, Components.interfaces.nsILDAPConnection.VERSION3);

  } catch (ex) {
    dump(ex);
    dump(" exception creating ldap connection\n");
    window.close();
  }
}

function stopFetching()
{
  if (gLdapOperation) {
    try {
      gLdapOperation.abandon();
    }
    catch (e) {
    }
  }
  window.close();
}

function importCert(ber_value)
{
  if (!gCertDB) {
    gCertDB = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  }

  var cert_length = new Object();
  var cert_bytes = ber_value.get(cert_length);

  if (cert_bytes) {
    gCertDB.importEmailCertificate(cert_bytes, cert_length.value, null);
  }
}

function getLDAPOperation()
{
    gLdapOperation = Components.classes["@mozilla.org/network/ldap-operation;1"]
      .createInstance().QueryInterface(Components.interfaces.nsILDAPOperation);

    gLdapOperation.init(gLdapConnection,
                        getProxyOnUIThread(new ldapMessageListener(),
                            Components.interfaces.nsILDAPMessageListener),
                        null);
}
function kickOffBind()
{
  try {
    getLDAPOperation();
    gLdapOperation.simpleBind(null);
  }
  catch (e) {
    window.close();
  }
}

function kickOffSearch()
{
  try {
    var prefix1 = "";
    var suffix1 = "";

    var urlFilter = gLdapServerURL.filter;

    if (urlFilter != null && urlFilter.length > 0 && urlFilter != "(objectclass=*)") {
      if (urlFilter[0] == '(') {
        prefix1 = "(&" + urlFilter;
      }
      else {
        prefix1 = "(&(" + urlFilter + ")";
      }
      suffix1 = ")";
    }

    var prefix2 = "";
    var suffix2 = "";

    if (gEmailAddresses.length > 1) {
      prefix2 = "(|";
      suffix2 = ")";
    }

    var mailFilter = "";

    for (var i = 0; i < gEmailAddresses.length; ++i) {
      mailFilter += "(mail=" + gEmailAddresses[i] + ")";
    }

    var filter = prefix1 + prefix2 + mailFilter + suffix2 + suffix1;

    var wanted_attributes = new Array();
    wanted_attributes[0] = CertAttribute;

    // Max search results =>
    // Double number of email addresses, because each person might have
    // multiple certificates listed. We expect at most two certificates,
    // one for signing, one for encrypting.
    // Maybe that number should be larger, to allow for deployments,
    // where even more certs can be stored per user???

    var maxEntriesWanted = gEmailAddresses.length * 2;

    getLDAPOperation();
    gLdapOperation.searchExt(gLdapServerURL.dn, gLdapServerURL.scope,
                             filter, 1, wanted_attributes, 0, maxEntriesWanted);
  }
  catch (e) {
    window.close();
  }
}


function boundListener() {
}

boundListener.prototype.QueryInterface =
  function(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsILDAPMessageListener))
        return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }

boundListener.prototype.onLDAPMessage =
  function(aMessage) {
  }

boundListener.prototype.onLDAPInit =
  function(aConn, aStatus) {
    kickOffBind();
  }


function ldapMessageListener() {
}

ldapMessageListener.prototype.QueryInterface =
  function(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsILDAPMessageListener))
        return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }

ldapMessageListener.prototype.onLDAPMessage =
  function(aMessage) {
    if (Components.interfaces.nsILDAPMessage.RES_SEARCH_RESULT == aMessage.type) {
      window.close();
      return;
    }

    if (Components.interfaces.nsILDAPMessage.RES_BIND == aMessage.type) {
      if (Components.interfaces.nsILDAPErrors.SUCCESS != aMessage.errorCode) {
        window.close();
      }
      else {
        kickOffSearch();
      }
      return;
    }

    if (Components.interfaces.nsILDAPMessage.RES_SEARCH_ENTRY == aMessage.type) {
      var outSize = new Object();
      try {
        var outBinValues = aMessage.getBinaryValues(CertAttribute, outSize);

        var i;
        for (i=0; i < outSize.value; ++i) {
          importCert(outBinValues[i]);
        }
      }
      catch (e) {
      }
      return;
    }
  }

ldapMessageListener.prototype.onLDAPInit =
  function(aConn, aStatus) {
  }


function getProxyOnUIThread(aObject, aInterface) {
    var eventQSvc = Components.
            classes["@mozilla.org/event-queue-service;1"].
            getService(Components.interfaces.nsIEventQueueService);

    var uiQueue = eventQSvc.
            getSpecialEventQueue(Components.interfaces.
            nsIEventQueueService.UI_THREAD_EVENT_QUEUE);

    var proxyMgr = Components.
            classes["@mozilla.org/xpcomproxy;1"].
            getService(Components.interfaces.nsIProxyObjectManager);

    return proxyMgr.getProxyForObject(uiQueue,
            aInterface, aObject, 5);
    // 5 == PROXY_ALWAYS | PROXY_SYNC
}
