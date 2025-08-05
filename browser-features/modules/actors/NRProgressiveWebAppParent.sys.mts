/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
