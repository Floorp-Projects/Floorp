/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRProfileManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRProfileManagerChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRProfileManager 5183 ! or Chrome Page!");
      Cu.exportFunction(this.NRGetCurrentProfile.bind(this), window, {
        defineAs: "NRGetCurrentProfile",
      });
    }
  }

  NRGetCurrentProfile(callback: (profile: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetCurrentProfile = resolve;
    });
    this.sendAsyncMessage("NRProfileManager:GetCurrentProfile");
    promise.then((profile) => callback(profile));
  }

  resolveGetCurrentProfile: ((profile: string) => void) | null = null;
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRProfileManager:GetCurrentProfile": {
        this.resolveGetCurrentProfile?.(message.data);
        this.resolveGetCurrentProfile = null;
        break;
      }
    }
  }
}
