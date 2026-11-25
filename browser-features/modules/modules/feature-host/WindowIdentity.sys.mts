// SPDX-License-Identifier: MPL-2.0

const identityKey = Symbol.for("floorp.featureHub.windowIdentityMap");
type IdentityGlobal = typeof globalThis & { [key: symbol]: unknown };
const identityGlobal = globalThis as IdentityGlobal;
let identities = identityGlobal[identityKey] as
  | WeakMap<Window, string>
  | undefined;
if (!identities) {
  identities = new WeakMap<Window, string>();
  Object.defineProperty(identityGlobal, identityKey, { value: identities });
}

const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

export function getFeatureWindowIdentity(window: Window): string {
  let identity = identities!.get(window);
  if (!identity) {
    identity = Services.uuid.generateUUID().toString();
    identities!.set(window, identity);
  }
  return identity;
}

function isWebPanelWindow(window: Window): boolean {
  if (
    (window as Window & { floorpWebPanelWindow?: boolean }).floorpWebPanelWindow
  ) return true;
  try {
    return new URL(window.location.href).searchParams.has("floorpWebPanelId");
  } catch {
    return true;
  }
}

export function isCandidateFeatureWindow(window: Window): boolean {
  if (window.closed || !window.document?.documentElement) return false;
  return window.document.documentElement.getAttribute("windowtype") ===
      "navigator:browser" &&
    !PrivateBrowsingUtils.isWindowPrivate(window) &&
    !isWebPanelWindow(window);
}

export function isEligibleFeatureWindow(window: Window): boolean {
  return isCandidateFeatureWindow(window) && (window as Window & {
        toolbar?: { visible?: boolean };
      }).toolbar?.visible === true;
}

export function isEligibleM0MainWindowForTests(
  windowType: string | null,
  toolbarVisible: boolean,
  isPrivate: boolean,
  isWebPanel: boolean,
): boolean {
  return windowType === "navigator:browser" && toolbarVisible && !isPrivate &&
    !isWebPanel;
}
