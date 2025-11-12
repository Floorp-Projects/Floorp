/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "../type.ts";

export const MAC_LAUNCH_METADATA_TOPIC = "floorp.mac.ssb.launch";
export const MAC_WINDOW_READY_TOPIC = "floorp.mac.ssb.window-ready";

type SanitizedMetadata = {
  id: string;
  name: string;
  start_url: string;
  icon?: string;
  protocol_handlers: Manifest["protocol_handlers"];
  file_handlers: Manifest["file_handlers"];
};

function sanitizeManifest(manifest: Manifest): SanitizedMetadata {
  return {
    id: manifest.id,
    name: manifest.name,
    start_url: manifest.start_url,
    icon: manifest.icon,
    protocol_handlers: manifest.protocol_handlers ?? [],
    file_handlers: manifest.file_handlers ?? [],
  };
}

function freezeMetadata(metadata: SanitizedMetadata): SanitizedMetadata {
  let cloned: SanitizedMetadata;
  const cloneFn = (
    globalThis as unknown as {
      structuredClone?: <T>(value: T) => T;
    }
  ).structuredClone;
  if (typeof cloneFn === "function") {
    cloned = cloneFn(metadata);
  } else {
    cloned = JSON.parse(JSON.stringify(metadata)) as SanitizedMetadata;
  }
  return Object.freeze(cloned);
}

export class MacIntegrationSupport {
  notifyLaunchRequest(manifest: Manifest, initialLaunch: boolean) {
    try {
      const payload = {
        ...sanitizeManifest(manifest),
        initialLaunch,
      };
      Services.obs.notifyObservers(
        null,
        MAC_LAUNCH_METADATA_TOPIC,
        JSON.stringify(payload),
      );
    } catch (error) {
      console.error("Failed to notify mac launch metadata", error);
    }
  }

  attachWindowMetadata(win: Window, manifest: Manifest) {
    try {
      const metadata = freezeMetadata(sanitizeManifest(manifest));
      // Expose metadata for in-process consumers (e.g. window chrome code).
      Object.defineProperty(win, "floorpSsbMetadata", {
        value: metadata,
        configurable: true,
        enumerable: false,
        writable: false,
      });

      Services.obs.notifyObservers(
        null,
        MAC_WINDOW_READY_TOPIC,
        JSON.stringify({
          ...metadata,
          windowId: win.windowGlobalChild?.innerWindowId ?? null,
        }),
      );
    } catch (error) {
      console.error("Failed to attach mac SSB metadata to window", error);
    }
  }
}
