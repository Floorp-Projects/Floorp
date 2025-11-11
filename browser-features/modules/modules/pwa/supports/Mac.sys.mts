/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { MacShimInfo, Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

type ExtendedMacSSBSupport = nsIMacSSBSupport & {
  getBundleInfo(
    aId: string,
    aName: string,
    bundlePath: { value?: string },
    bundleIdentifier: { value?: string },
  ): void;
};

type CiWithMacSSB = typeof Ci & {
  nsIMacSSBSupport: nsIID;
};

function getMacSSBService(): nsIMacSSBSupport | null {
  try {
    return Cc["@mozilla.org/widget/mac-ssb-support;1"].getService(
      (Ci as CiWithMacSSB).nsIMacSSBSupport,
    ) as nsIMacSSBSupport;
  } catch (error) {
    console.error("Failed to acquire nsIMacSSBSupport", error);
    return null;
  }
}

function getMacDockSupport(): nsIMacDockSupport | null {
  try {
    return Cc["@mozilla.org/widget/macdocksupport;1"].getService(
      Ci.nsIMacDockSupport,
    );
  } catch (error) {
    console.error("Failed to acquire nsIMacDockSupport", error);
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
      await this.updateMacShimInfo(ssb);
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

    if (ssb.macShim) {
      delete ssb.macShim;
    }

    await this.updateMacShimInfo(ssb);
  }

  async applyOSIntegration(ssb: Manifest, _aWindow: Window | null) {
    const service = getMacSSBService();
    if (!service) {
      return;
    }

    await this.updateMacShimInfo(ssb);

    const iconContainer = await loadIconContainer(ssb.icon);
    try {
      service.applyDockIntegration(ssb.id, ssb.name, iconContainer);
    } catch (error) {
      console.error("macOS SSB dock integration failed", error);
    }
  }

  async launch(ssb: Manifest): Promise<boolean> {
    const shimInfo = await this.ensureMacShimAvailable(ssb);
    if (!shimInfo) {
      return false;
    }

    const dock = getMacDockSupport();
    if (!dock) {
      return false;
    }

    try {
      const bundleFile = Cc["@mozilla.org/file/local;1"].createInstance(
        Ci.nsIFile,
      );
      bundleFile.initWithPath(shimInfo.bundlePath);
      dock.launchAppBundle(bundleFile, []);
      return true;
    } catch (error) {
      console.error("macOS SSB launch failed", error);
    }

    return false;
  }

  private async updateMacShimInfo(ssb: Manifest): Promise<MacShimInfo | null> {
    const service = getMacSSBService() as ExtendedMacSSBSupport | null;
    if (!service) {
      return null;
    }

    const bundlePath: { value?: string } = {};
    const bundleIdentifier: { value?: string } = {};
    try {
      service.getBundleInfo(ssb.id, ssb.name, bundlePath, bundleIdentifier);
    } catch (error) {
      console.error("macOS SSB getBundleInfo failed", error);
      return null;
    }

    const resolvedPath = bundlePath.value ?? "";
    if (!resolvedPath) {
      if (ssb.macShim) {
        delete ssb.macShim;
      }
      return null;
    }

    const exists = await IOUtils.exists(resolvedPath);
    if (!exists) {
      if (ssb.macShim) {
        delete ssb.macShim;
      }
      return null;
    }

    const shimInfo: MacShimInfo = {
      bundlePath: resolvedPath,
      bundleId: bundleIdentifier.value ?? "",
      lastUpdated: Date.now(),
    };
    ssb.macShim = shimInfo;
    return shimInfo;
  }

  private async ensureMacShimAvailable(
    ssb: Manifest,
  ): Promise<MacShimInfo | null> {
    if (ssb.macShim) {
      const exists = await IOUtils.exists(ssb.macShim.bundlePath);
      if (exists) {
        return ssb.macShim;
      }
    }

    return await this.updateMacShimInfo(ssb);
  }
}
