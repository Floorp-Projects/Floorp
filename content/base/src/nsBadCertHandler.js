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
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Daniel Veditz <dveditz@mozilla.com>
 *  Manish Singh <manish@flock.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * This component's job is to prevent "bad cert" security dialogs from
 * being shown to the user when in XHR backgroundRequest mode. This
 * causes the request to simply fail if the certificate is bad.
 */
function BadCertHandler() {
}

BadCertHandler.prototype = {

  // Suppress any certificate errors
  notifyCertProblem: function(socketInfo, status, targetSite) {
    return true;
  },

  // Suppress any ssl errors
  notifySSLError: function(socketInfo, error, targetSite) {
    return true;
  },

  // nsIInterfaceRequestor
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBadCertListener2,
                                         Ci.nsISSLErrorListener,
                                         Ci.nsIInterfaceRequestor]),

  classDescription: "XMLHttpRequest Bad Cert Handler",
  classID:          Components.ID("{dbded6ec-edbf-4054-a834-287b82c260f9}"),
  contractID:       "@mozilla.org/content/xmlhttprequest-bad-cert-handler;1"
};

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([BadCertHandler]);
}
