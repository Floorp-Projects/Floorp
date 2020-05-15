/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["AboutNewInstallChild"];

const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

class AboutNewInstallChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();

    this.exportFunctions(["RPMGetUpdateChannel", "RPMGetFxAccountsEndpoint"]);
  }

  RPMGetUpdateChannel() {
    return UpdateUtils.UpdateChannel;
  }

  RPMGetFxAccountsEndpoint(aEntrypoint) {
    return this.wrapPromise(
      new Promise(resolve => {
        this.sendQuery("FxAccountsEndpoint", { entrypoint: aEntrypoint }).then(
          result => {
            resolve(Cu.cloneInto(result, this.contentWindow));
          }
        );
      })
    );
  }
}
