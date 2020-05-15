/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm"
);

var EXPORTED_SYMBOLS = ["AboutNewInstallParent"];

class AboutNewInstallParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "FxAccountsEndpoint":
        return FxAccounts.config.promiseConnectAccountURI(
          message.data.entrypoint
        );
    }

    return undefined;
  }
}
