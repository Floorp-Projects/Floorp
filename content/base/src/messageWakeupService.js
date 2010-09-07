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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alon Zakai <azakai@mozilla.com>
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

const CATEGORY_WAKEUP_REQUEST = "wakeup-request";

function MessageWakeupService() { };

MessageWakeupService.prototype =
{
  classID:          Components.ID("{f9798742-4f7b-4188-86ba-48b116412b29}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIMessageWakeupService, Ci.nsISupports, Ci.nsIObserver]),

  messagesData: [],

  get messageManager() {
    if (!this._messageManager)
      this._messageManager = Cc["@mozilla.org/parentprocessmessagemanager;1"].
                             getService(Ci.nsIFrameMessageManager);
    return this._messageManager;
  },

  requestWakeup: function(aMessageName, aCid, aIid, aMethod) {
    this.messagesData[aMessageName] = {
      cid: aCid,
      iid: aIid,
      method: aMethod,
    };

    this.messageManager.addMessageListener(aMessageName, this);
  },

  receiveMessage: function(aMessage) {
    var data = this.messagesData[aMessage.name];
    delete this.messagesData[aMessage.name];
    var service = Cc[data.cid][data.method](Ci[data.iid]).
                  wrappedJSObject;
// TODO: When bug 593407 is ready, stop doing the wrappedJSObject hack
//       and use the line below instead
//                  QueryInterface(Ci.nsIFrameMessageListener);
    this.messageManager.addMessageListener(aMessage.name, service);
    this.messageManager.removeMessageListener(aMessage.name, this);
    service.receiveMessage(aMessage);
  },

  observe: function TM_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "profile-after-change":
        {
          var catMan = Cc["@mozilla.org/categorymanager;1"].
                           getService(Ci.nsICategoryManager);
          var entries = catMan.enumerateCategory(CATEGORY_WAKEUP_REQUEST);
          while (entries.hasMoreElements()) {
            var entry = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
            var value = catMan.getCategoryEntry(CATEGORY_WAKEUP_REQUEST, entry);
            var parts = value.split(",");
            var cid = parts[0];
            var iid = parts[1];
            var method = parts[2];
            var messages = parts.slice(3);
            messages.forEach(function(messageName) {
              this.requestWakeup(messageName, cid, iid, method);
            }, this);
          }
        }
        break;
    }
  },
};

var components = [MessageWakeupService];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

