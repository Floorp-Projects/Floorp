/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is the Places protocol handler.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
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

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const SCHEME = "place";
const URL = "chrome://browser/content/places/content-ui/controller.xhtml";

function PlacesProtocolHandler() {}

PlacesProtocolHandler.prototype = {
  scheme: SCHEME,
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
                 Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE |
                 Ci.nsIProtocolHandler.URI_NORELATIVE |
                 Ci.nsIProtocolHandler.URI_NOAUTH,

  newURI: function PPH_newURI(aSpec, aOriginCharset, aBaseUri) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel: function PPH_newChannel(aUri) {
    let chan = NetUtil.newChannel(URL);
    chan.originalURI = aUri;
    return chan;
  },

  allowPort: function PPH_allowPort(aPort, aScheme) {
    return false;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIProtocolHandler
  ]),

  classID: Components.ID("{6bcb9bde-9018-4443-a071-c32653469597}")
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([PlacesProtocolHandler]);
