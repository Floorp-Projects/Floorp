/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PrivateContainer } from "#features-chrome/common/private-container/PrivateContainer.ts";

export const overrides = [
  () => {
    const gBrowser = globalThis.gBrowser;
    if (!gBrowser || typeof gBrowser.createBrowser !== "function") {
      console.warn(
        "[FloorpPrivateContainer] gBrowser.createBrowser not available for override",
      );
      return;
    }

    const originalCreateBrowser = gBrowser.createBrowser.bind(gBrowser);

    gBrowser.createBrowser = function (...args: unknown[]) {
      const browser = originalCreateBrowser(...args);
      if (!browser) return browser;

      try {
        const options = args[0] as { userContextId?: number } | undefined;
        const privateContainerUserContextId =
          PrivateContainer.getPrivateContainerUserContextId();

        if (
          options?.userContextId != null &&
          options.userContextId === privateContainerUserContextId
        ) {
          browser.setAttribute("disableglobalhistory", "true");
          browser.setAttribute("disablehistory", "true");
        }
      } catch (e) {
        console.error(
          "[FloorpPrivateContainer] Error in createBrowser override:",
          e,
        );
      }

      return browser;
    };
  },
];
