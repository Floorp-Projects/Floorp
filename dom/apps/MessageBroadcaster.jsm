/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Manages registration of message managers from child processes and
// broadcasting messages to them.

this.EXPORTED_SYMBOLS = ["MessageBroadcaster"];

this.MessageBroadcaster = {
  appGetter: null,
  children: [],

  init: function(aAppGetter) {
    if (!aAppGetter || typeof aAppGetter !== "function") {
      throw "MessageBroadcaster.init needs a function parameter";
    }
    this.appGetter = aAppGetter;
  },

  // We manage refcounting of listeners per message manager.
  addMessageListener: function(aMsgNames, aApp, aMm) {
    aMsgNames.forEach(aMsgName => {
      let manifestURL = aApp && aApp.manifestURL;
      if (!(aMsgName in this.children)) {
        this.children[aMsgName] = [];
      }

      let mmFound = this.children[aMsgName].some(mmRef => {
        if (mmRef.mm === aMm) {
          mmRef.refCount++;
          return true;
        }
        return false;
      });

      if (!mmFound) {
        this.children[aMsgName].push({
          mm: aMm,
          refCount: 1
        });
      }

      // If the state reported by the registration is outdated, update it now.
      if (manifestURL && ((aMsgName === 'Webapps:FireEvent') ||
          (aMsgName === 'Webapps:UpdateState'))) {
        let app = this.appGetter(aApp.manifestURL);
        if (app && ((aApp.installState !== app.installState) ||
                    (aApp.downloading !== app.downloading))) {
          debug("Got a registration from an outdated app: " +
                manifestURL);
          let aEvent ={
            type: app.installState,
            app: app,
            manifestURL: app.manifestURL,
            manifest: app.manifest
          };
          aMm.sendAsyncMessage(aMsgName, aEvent);
        }
      }
    });
  },

  removeMessageListener: function(aMsgNames, aMm) {
    if (aMsgNames.length === 1 &&
        aMsgNames[0] === "Webapps:Internal:AllMessages") {
      for (let msgName in this.children) {
        let msg = this.children[msgName];

        for (let mmI = msg.length - 1; mmI >= 0; mmI -= 1) {
          let mmRef = msg[mmI];
          if (mmRef.mm === aMm) {
            msg.splice(mmI, 1);
          }
        }

        if (msg.length === 0) {
          delete this.children[msgName];
        }
      }
      return;
    }

    aMsgNames.forEach(aMsgName => {
      if (!(aMsgName in this.children)) {
        return;
      }

      let removeIndex;
      this.children[aMsgName].some((mmRef, index) => {
        if (mmRef.mm === aMm) {
          mmRef.refCount--;
          if (mmRef.refCount === 0) {
            removeIndex = index;
          }
          return true;
        }
        return false;
      });

      if (removeIndex) {
        this.children[aMsgName].splice(removeIndex, 1);
      }
    });
  },

  // Some messages can be listened by several content processes:
  // Webapps:AddApp
  // Webapps:RemoveApp
  // Webapps:Install:Return:OK
  // Webapps:Uninstall:Return:OK
  // Webapps:Uninstall:Broadcast:Return:OK
  // Webapps:FireEvent
  // Webapps:checkForUpdate:Return:OK
  // Webapps:UpdateState
  broadcastMessage: function(aMsgName, aContent) {
    if (!(aMsgName in this.children)) {
      return;
    }
    this.children[aMsgName].forEach((mmRef) => {
      mmRef.mm.sendAsyncMessage(aMsgName, this.formatMessage(aContent));
    });
  },

  formatMessage: function(aData) {
    let msg = aData;
    delete msg["mm"];
    return msg;
  },
}
