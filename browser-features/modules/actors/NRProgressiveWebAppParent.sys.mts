/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRProgressiveWebAppParent extends JSWindowActorParent {
  // deno-lint-ignore require-await
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "ProgressiveWebApp:CheckPageHasManifest":
        return this.sendNotifyToPageAction();
    }
    return null;
  }

  sendNotifyToPageAction() {
    const context = this.browsingContext;
    // Subframe pageshow must not refresh the top document's install action.
    // Ignore a queued message from a document that has already navigated away.
    if (
      !context || context !== context.top ||
      this.manager !== context.currentWindowGlobal
    ) {
      return;
    }
    const browser = context.embedderElement;
    if (browser) {
      Services.obs.notifyObservers(
        browser as unknown as nsISupports,
        "nora-pwa-check-page-has-manifest",
      );
    }
  }
}
