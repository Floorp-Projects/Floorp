/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRAppConstantsParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "AppConstants:GetConstants": {
        const { AppConstants } = ChromeUtils.importESModule(
          "resource://gre/modules/AppConstants.sys.mjs",
        );

        this.sendAsyncMessage(
          "AppConstants:GetConstants",
          JSON.stringify(AppConstants),
        );
      }
    }
  }
}
