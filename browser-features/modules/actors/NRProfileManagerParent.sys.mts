//@ts-ignore: decorator
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRProfileManagerParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRProfileManager:GetCurrentProfile": {
        // const { SelectableProfileService } = ChromeUtils.importESModule(
        //   "resource:///modules/profiles/SelectableProfileService.sys.mjs",
        // );
        // const profile = SelectableProfileService.currentProfile;

        // Get profile directory
        const dirService = Components
          .classes[
            "@mozilla.org/file/directory_service;1" as keyof typeof Components.classes
          ]
          ?.getService(Components.interfaces.nsIProperties);

        const profileDir = dirService.get(
          "ProfD",
          Components.interfaces.nsIFile,
        );
        const profilePath = profileDir.path;
        const profileName = profileDir.leafName;

        this.sendAsyncMessage(
          "NRProfileManager:GetCurrentProfile",
          JSON.stringify({
            profileName,
            profilePath,
          }),
        );
        break;
      }
    }
  }
}
