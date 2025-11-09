/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

function getMacSSBService(): nsIMacSSBSupport | null {
  try {
    return Cc["@mozilla.org/widget/mac-ssb-support;1"].getService(
      Ci.nsIMacSSBSupport,
    );
  } catch (error) {
    console.error("Failed to acquire nsIMacSSBSupport", error);
    return null;
  }
}

async function loadIconContainer(icon: string | undefined) {
  if (!icon) {
    return null;
  }

  try {
    const { container } = await ImageTools.loadImage(Services.io.newURI(icon));
    return container;
  } catch (error) {
    console.error("Failed to decode SSB icon", error);
    return null;
  }
}

export class MacSupport {
  async install(ssb: Manifest) {
    const service = getMacSSBService();
    if (!service) {
      return;
    }

    const iconContainer = await loadIconContainer(ssb.icon);
    try {
      service.install(ssb.id, ssb.name, iconContainer);
    } catch (error) {
      console.error("macOS SSB install failed", error);
    }
  }

  async uninstall(ssb: Manifest) {
    const service = getMacSSBService();
    if (!service) {
      return;
    }

    try {
      service.uninstall(ssb.id, ssb.name);
    } catch (error) {
      console.error("macOS SSB uninstall failed", error);
    }
  }

  async applyOSIntegration(ssb: Manifest, _aWindow: Window) {
    const service = getMacSSBService();
    if (!service) {
      return;
    }

    const iconContainer = await loadIconContainer(ssb.icon);
    try {
      service.applyDockIntegration(ssb.id, ssb.name, iconContainer);
    } catch (error) {
      console.error("macOS SSB dock integration failed", error);
    }
  }
}
