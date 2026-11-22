// SPDX-License-Identifier: MPL-2.0

import type {
  HubDomProbeCommand,
  HubDomProbeRead,
  HubDomProbeTestTab,
} from "./HubDomProbeTypes.ts";

export const HUB_DOM_PROBE_ACTOR = "M1HubDomProbe";
export const HUB_CONNECT_BLOCK_QUERY = "?m1-hub-probe=block-connect";
export const HUB_ACK_BLOCK_QUERY = "?m1-hub-probe=block-ack";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export function registerHubDomProbe(): void {
  const startupMode = Services.prefs.getStringPref("nora.startup.mode", "");
  if (startupMode !== "dev" && startupMode !== "test") {
    throw new Error("the HUB DOM probe is limited to dev/test startup");
  }
  const options: WindowActorOptions = {
    parent: {
      esModuleURI:
        "resource://noraneko/modules/feature-host/test/M1HubDomProbeParent.sys.mjs",
    },
    child: {
      esModuleURI:
        "resource://noraneko/modules/feature-host/test/M1HubDomProbeChild.sys.mjs",
      events: {
        DOMDocElementInserted: {},
        DOMContentLoaded: {},
        pageshow: {},
      },
    },
    matches: [
      "http://localhost/*",
      "http://127.0.0.1/*",
      "chrome://noraneko-settings/*",
      "about:hub*",
    ],
    allFrames: false,
  };
  if (Services.prefs.getBoolPref("nora.dev.allow_http_loader", false)) {
    options.remoteTypes = [
      "web",
      "webIsolated",
      "webCOOP+COEP",
      "file",
      "privilegedabout",
      "parent",
    ];
    options.safeForUntrustedWebProcess = true;
  }
  ChromeUtils.registerWindowActor(HUB_DOM_PROBE_ACTOR, options);
}

export async function sendHubDomProbeCommand(
  tab: HubDomProbeTestTab,
  command: HubDomProbeCommand,
  data?: string,
): Promise<unknown> {
  const actor = tab.linkedBrowser.browsingContext.currentWindowGlobal?.getActor(
    HUB_DOM_PROBE_ACTOR,
  );
  if (!actor) throw new Error("the HUB DOM probe actor is not available");
  let timeout: number | undefined;
  try {
    return await Promise.race([
      actor.sendQuery(`${HUB_DOM_PROBE_ACTOR}:${command}`, data),
      new Promise<never>((_resolve, reject) => {
        timeout = setTimeout(
          () => reject(new Error("the HUB DOM probe query timed out")),
          2_000,
        );
      }),
    ]);
  } finally {
    if (timeout !== undefined) clearTimeout(timeout);
  }
}

export async function readHubDomProbe(
  tab: HubDomProbeTestTab,
): Promise<HubDomProbeRead> {
  const response = await sendHubDomProbeCommand(tab, "Read");
  if (typeof response !== "string") {
    throw new Error("the HUB DOM probe returned a non-string response");
  }
  const parsed: unknown = JSON.parse(response);
  if (!parsed || typeof parsed !== "object") {
    throw new Error("the HUB DOM probe returned invalid JSON");
  }
  return parsed as HubDomProbeRead;
}

export async function waitForHubDomProbe(
  tab: HubDomProbeTestTab,
  predicate: (probe: HubDomProbeRead) => boolean,
  label: string,
  timeoutMs = 10_000,
): Promise<HubDomProbeRead> {
  const deadline = Date.now() + timeoutMs;
  let last = "no probe response";
  while (Date.now() < deadline) {
    try {
      const probe = await readHubDomProbe(tab);
      last = JSON.stringify(probe);
      if (predicate(probe)) return probe;
    } catch (error) {
      last = String(error);
    }
    await new Promise<void>((resolve) => setTimeout(resolve, 20));
  }
  throw new Error(`${label} timed out; last probe: ${last}`);
}
