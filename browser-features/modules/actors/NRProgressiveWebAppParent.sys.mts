// SPDX-License-Identifier: MPL-2.0

export class NRProgressiveWebAppParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "ProgressiveWebApp:CheckPageHasManifest":
        return this.sendNotifyToPageAction();
    }
    return null;
  }

  sendNotifyToPageAction() {
    Services.obs.notifyObservers({}, "nora-pwa-check-page-has-manifest");
  }
}
