/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class AboutMessagePreviewChild extends JSWindowActorChild {
  handleEvent(event) {
    console.log(`Received page event ${event.type}`);
  }

  actorCreated() {
    this.exportFunctions();
  }

  exportFunctions() {
    const window = this.contentWindow;

    Cu.exportFunction(this.MPShowMessage.bind(this), window, {
      defineAs: "MPShowMessage",
    });
  }

  MPShowMessage(message) {
    this.sendAsyncMessage(`MessagePreview:SHOW_MESSAGE`, message);
  }
}
