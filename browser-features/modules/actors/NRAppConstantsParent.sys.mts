// SPDX-License-Identifier: MPL-2.0




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
