/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const { NoranekoConstants } = ChromeUtils.importESModule(
  "resource://noraneko/modules/NoranekoConstants.sys.mjs",
);

const PREF_OLD_VERSION2 = "floorp.startup.oldVersion2";
// Legacy preference from older Floorp versions (pre-version2 system)
const PREF_OLD_VERSION_LEGACY = "floorp.startup.oldVersion";

// nsIUpdateChecker constants for getUpdateURL()
// Value 1 represents foreground check priority
const UPDATE_CHECK_TYPE_FOREGROUND = 1;

export type UpdateType = "major" | "minor" | "patch" | null;

export interface UpdateInfo {
  isFirstRun: boolean;
  isUpdated: boolean;
  updateType: UpdateType;
  oldVersion: string | null;
  newVersion: string;
}

export interface RemoteUpdateInfo {
  appVersion: string;
  appVersion2: string;
  buildID: string;
  buildID2: string;
  displayVersion: string;
  detailsURL: string;
  patchURL: string;
  patchSize: number;
  patchType: string;
  patchHashFunction: string;
  patchHashValue: string;
  updateType: string; // "major" or "minor" from XML
}

export interface Version2UpdateStatus {
  hasUpdate: boolean;
  updateType: UpdateType;
  localVersion2: string;
  localBuildID2: string;
  remoteVersion2: string | null;
  remoteBuildID2: string | null;
  detailsURL: string | null;
}

/**
 * Parse version string into components.
 * e.g., "12.8.1" -> { major: 12, minor: 8, patch: 1 }
 */
function parseVersion(version: string): {
  major: number;
  minor: number;
  patch: number;
} | null {
  const match = version.match(/^(\d+)\.(\d+)(?:\.(\d+))?/);
  if (!match) return null;
  return {
    major: parseInt(match[1], 10),
    minor: parseInt(match[2], 10),
    patch: parseInt(match[3] ?? "0", 10),
  };
}

/**
 * Compare two versions and return:
 *  1 if newVersion > oldVersion (upgrade)
 *  0 if newVersion === oldVersion
 * -1 if newVersion < oldVersion (downgrade)
 */
function compareVersions(oldVersion: string, newVersion: string): number {
  const oldParsed = parseVersion(oldVersion);
  const newParsed = parseVersion(newVersion);

  if (!oldParsed || !newParsed) return 0;

  if (newParsed.major !== oldParsed.major) {
    return newParsed.major > oldParsed.major ? 1 : -1;
  }
  if (newParsed.minor !== oldParsed.minor) {
    return newParsed.minor > oldParsed.minor ? 1 : -1;
  }
  if (newParsed.patch !== oldParsed.patch) {
    return newParsed.patch > oldParsed.patch ? 1 : -1;
  }

  return 0;
}

/**
 * Determine the type of update based on version comparison.
 * Returns null if not an upgrade (same version or downgrade).
 */
function getUpdateType(oldVersion: string, newVersion: string): UpdateType {
  const comparison = compareVersions(oldVersion, newVersion);

  // Only report update type for upgrades
  if (comparison !== 1) return null;

  const oldParsed = parseVersion(oldVersion);
  const newParsed = parseVersion(newVersion);

  if (!oldParsed || !newParsed) return null;

  if (newParsed.major > oldParsed.major) {
    return "major";
  }
  if (newParsed.minor > oldParsed.minor) {
    return "minor";
  }
  if (newParsed.patch > oldParsed.patch) {
    return "patch";
  }

  return null;
}

/**
 * Get the update.xml URL using nsIUpdateChecker.
 * This respects MOZ_UPDATE_URL and other Firefox update settings.
 */
async function getUpdateXmlUrl(): Promise<string> {
  const checker = Cc["@mozilla.org/updates/update-checker;1"].getService(
    Ci.nsIUpdateChecker,
  );
  const url = await checker.getUpdateURL(UPDATE_CHECK_TYPE_FOREGROUND);

  // Sanitize URL for logging - remove query parameters that might contain sensitive data
  try {
    const urlObj = new URL(url);
    const sanitizedUrl = urlObj.origin + urlObj.pathname;
    console.log(
      `[NoranekoUpdateChecker] Using nsIUpdateChecker URL: ${sanitizedUrl}`,
    );
  } catch {
    // If URL parsing fails, just log without URL details
    console.log("[NoranekoUpdateChecker] Using nsIUpdateChecker URL");
  }
  return url;
}

/**
 * Fetch and parse update.xml from the server.
 */
export async function fetchRemoteUpdateInfo(): Promise<RemoteUpdateInfo | null> {
  try {
    const url = await getUpdateXmlUrl();
    const response = await fetch(url);

    if (!response.ok) {
      console.error(
        `[NoranekoUpdateChecker] Failed to fetch update.xml: ${response.status}`,
      );
      return null;
    }

    const xmlText = await response.text();
    return parseUpdateXml(xmlText);
  } catch (error) {
    console.error("[NoranekoUpdateChecker] Error fetching update.xml:", error);
    return null;
  }
}

/**
 * Parse update.xml content to extract version info.
 */
function parseUpdateXml(xmlText: string): RemoteUpdateInfo | null {
  try {
    const parser = new DOMParser();
    const doc = parser.parseFromString(xmlText, "application/xml");

    // Check for parser errors
    const parserError = doc.querySelector("parsererror");
    if (parserError) {
      console.error(
        "[NoranekoUpdateChecker] XML parsing error:",
        parserError.textContent,
      );
      return null;
    }

    const updateElement = doc.querySelector("update");
    if (!updateElement) {
      console.error("[NoranekoUpdateChecker] No <update> element found in XML");
      return null;
    }

    const patchElement = doc.querySelector("patch");

    return {
      appVersion: updateElement.getAttribute("appVersion") ?? "",
      appVersion2: updateElement.getAttribute("appVersion2") ?? "",
      buildID: updateElement.getAttribute("buildID") ?? "",
      buildID2: updateElement.getAttribute("buildID2") ?? "",
      displayVersion: updateElement.getAttribute("displayVersion") ?? "",
      detailsURL: updateElement.getAttribute("detailsURL") ?? "",
      patchURL: patchElement?.getAttribute("URL") ?? "",
      patchSize: parseInt(patchElement?.getAttribute("size") ?? "0", 10),
      patchType: patchElement?.getAttribute("type") ?? "complete",
      patchHashFunction: patchElement?.getAttribute("hashFunction") ?? "",
      patchHashValue: patchElement?.getAttribute("hashValue") ?? "",
      updateType: updateElement.getAttribute("type") ?? "major",
    };
  } catch (error) {
    console.error("[NoranekoUpdateChecker] Error parsing update.xml:", error);
    return null;
  }
}

/**
 * Implementation of nsIUpdatePatch interface
 */
class NoranekoUpdatePatch {
  type: string;
  URL: string;
  hashFunction: string;
  hashValue: string;
  size: number;
  state: string;
  selected: boolean;

  constructor(info: RemoteUpdateInfo) {
    this.type = info.patchType;
    this.URL = info.patchURL;
    this.hashFunction = info.patchHashFunction;
    this.hashValue = info.patchHashValue;
    this.size = info.patchSize;
    this.state = "pending";
    this.selected = true;
    this.finalURL = "";
    this.errorCode = 0;
  }

  finalURL: string;
  errorCode: number;

  serialize(updates: Document): Element {
    // Return Element as expected by nsIUpdatePatch interface
    if (!updates || typeof updates.createElement !== "function") {
      throw new Error("Invalid document passed to serialize");
    }
    const patchElem = updates.createElement("patch");
    patchElem.setAttribute("type", this.type);
    patchElem.setAttribute("URL", this.URL);
    patchElem.setAttribute("hashFunction", this.hashFunction);
    patchElem.setAttribute("hashValue", this.hashValue);
    patchElem.setAttribute("size", this.size.toString());
    patchElem.setAttribute("selected", this.selected.toString());
    patchElem.setAttribute("state", this.state);
    return patchElem;
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIUpdatePatch]);
}

/**
 * Implementation of nsIUpdate interface
 */
class NoranekoUpdate {
  type: string;
  name: string;
  displayVersion: string;
  appVersion: string;
  buildID: string;
  detailsURL: string;
  isCompleteUpdate: boolean;
  installDate: number;
  statusText: string;
  showNeverForVersion: boolean;
  unsupported: boolean;
  patches: NoranekoUpdatePatch[];
  promptWaitTime: number;
  billboardURL: string;
  elevationFailureCount: number;
  errorCode: number;
  platformVersion: string;
  previousAppVersion: string;
  serviceURL: string;
  channel: string;
  originalURL: string;
  isOSUpdate: boolean;

  constructor(info: RemoteUpdateInfo) {
    this.type = info.updateType;
    this.name = info.displayVersion || info.appVersion2;
    this.displayVersion = info.displayVersion || info.appVersion2;
    this.appVersion = info.appVersion; // Keep original appVersion for compatibility
    this.buildID = info.buildID;
    this.detailsURL = info.detailsURL;
    this.isCompleteUpdate = info.patchType === "complete";
    this.installDate = 0;
    this.statusText = "";
    this.showNeverForVersion = false;
    this.unsupported = false;
    this.promptWaitTime = 0;
    this.billboardURL = "";
    this.elevationFailureCount = 0;
    this.errorCode = 0;
    this.platformVersion = info.appVersion;
    this.previousAppVersion = "";
    this.serviceURL = info.detailsURL;
    this.channel = "default";
    this.originalURL = "";
    this.isOSUpdate = false;
    this.state = "pending";
    this.elevationFailure = false;

    this.patches = [new NoranekoUpdatePatch(info)];
  }

  state: string;
  elevationFailure: boolean;

  serialize(updates: Document): Element {
    if (!updates || typeof updates.createElement !== "function") {
      throw new Error("Invalid document passed to serialize");
    }

    const updateElem = updates.createElement("update");
    updateElem.setAttribute("type", this.type);
    updateElem.setAttribute("name", this.name);
    updateElem.setAttribute("displayVersion", this.displayVersion);
    updateElem.setAttribute("appVersion", this.appVersion);
    updateElem.setAttribute("buildID", this.buildID);
    updateElem.setAttribute("detailsURL", this.detailsURL);

    for (const patch of this.patches) {
      const patchElem = patch.serialize(updates);
      if (patchElem) {
        updateElem.appendChild(patchElem);
      }
    }
    return updateElem;
  }

  get patchCount(): number {
    return this.patches.length;
  }

  getPatchAt(index: number): NoranekoUpdatePatch {
    return this.patches[index];
  }

  get selectedPatch(): NoranekoUpdatePatch {
    // We always have one patch selected in this implementation
    return this.patches.find((p) => p.selected) ?? this.patches[0];
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIUpdate]);
}

/**
 * Check for version2-based updates by comparing local and remote versions.
 */
export async function checkForVersion2Updates(): Promise<Version2UpdateStatus> {
  const localVersion2 = NoranekoConstants.version2 as string;
  const localBuildID2 = NoranekoConstants.buildID2 as string;

  const remoteInfo = await fetchRemoteUpdateInfo();

  if (!remoteInfo) {
    return {
      hasUpdate: false,
      updateType: null,
      localVersion2,
      localBuildID2,
      remoteVersion2: null,
      remoteBuildID2: null,
      detailsURL: null,
    };
  }

  const remoteVersion2 = remoteInfo.appVersion2;
  const remoteBuildID2 = remoteInfo.buildID2;

  // Validate remote version - if empty, treat as no update available
  if (!remoteVersion2) {
    console.warn(
      "[NoranekoUpdateChecker] Remote appVersion2 is empty, skipping update check",
    );
    return {
      hasUpdate: false,
      updateType: null,
      localVersion2,
      localBuildID2,
      remoteVersion2: null,
      remoteBuildID2: null,
      detailsURL: null,
    };
  }

  // Compare versions - only detect upgrades (not downgrades)
  // compareVersions(old, new) returns 1 if new > old (upgrade available)
  const versionComparison = compareVersions(localVersion2, remoteVersion2);
  const isVersionUpgrade = versionComparison === 1;

  // Only check buildID difference if both are provided
  // If remoteBuildID2 is empty, we can't reliably detect hotfixes
  const isBuildIDDifferent =
    !!remoteBuildID2 && localBuildID2 !== remoteBuildID2;

  // hasUpdate is true only if:
  // 1. Remote version is higher than local, OR
  // 2. Versions are same but buildID is different (rebuild/hotfix)
  const hasUpdate =
    isVersionUpgrade || (versionComparison === 0 && isBuildIDDifferent);

  // updateType classification:
  // - For version upgrades: returns "major", "minor", or "patch" based on semantic versioning
  // - For buildID-only changes (hotfixes/rebuilds): returns null (intentional)
  // This distinguishes between version updates and same-version rebuilds
  const updateType = isVersionUpgrade
    ? getUpdateType(localVersion2, remoteVersion2)
    : null;

  return {
    hasUpdate,
    updateType,
    localVersion2,
    localBuildID2,
    remoteVersion2,
    remoteBuildID2,
    detailsURL: remoteInfo.detailsURL,
  };
}

/**
 * Check for updates based on version2 (local pref comparison).
 * Returns information about the update state.
 *
 * Migration logic:
 * - If oldVersion2 exists: use it for comparison (normal case)
 * - If oldVersion2 is empty but legacy oldVersion exists: this is an existing user
 *   upgrading to the new version2 system - treat as updated, not first-run
 * - If both are empty: truly a first-run user
 */
export function checkIfUpdatedOnStartup(): UpdateInfo {
  const newVersion = NoranekoConstants.version2 as string;

  // Check for the new version2 preference
  const oldVersion2 = Services.prefs.getStringPref(PREF_OLD_VERSION2, "");

  // Check for legacy preference (old Floorp versions used this)
  const legacyOldVersion = Services.prefs.getStringPref(
    PREF_OLD_VERSION_LEGACY,
    "",
  );

  // Determine if this is truly a first run:
  // - If version2 pref exists, user has run this version before
  // - If legacy pref exists but version2 doesn't, this is an existing user upgrading
  // - If neither exists, this is a first-run user
  const hasVersion2Pref = oldVersion2 !== "";
  const hasLegacyPref = legacyOldVersion !== "";
  const isFirstRun = !hasVersion2Pref && !hasLegacyPref;

  if (isFirstRun) {
    return {
      isFirstRun: true,
      isUpdated: false,
      updateType: null,
      oldVersion: null,
      newVersion,
    };
  }

  // For existing users upgrading from legacy system:
  // - version2 pref is empty, but legacy pref exists
  // - Treat as an update from "unknown" to current version
  if (!hasVersion2Pref && hasLegacyPref) {
    console.log(
      `[NoranekoUpdateChecker] Migrating from legacy version system. Legacy version: ${legacyOldVersion}`,
    );
    return {
      isFirstRun: false,
      isUpdated: true,
      updateType: "minor", // Assume minor update for migration
      oldVersion: null, // We don't have comparable version2 from before
      newVersion,
    };
  }

  // Normal case: compare version2 preferences
  // Only treat as updated if it's an actual upgrade (not a downgrade)
  const versionComparison = compareVersions(oldVersion2, newVersion);
  const isUpdated = versionComparison === 1; // 1 = upgrade
  const updateType = isUpdated ? getUpdateType(oldVersion2, newVersion) : null;

  return {
    isFirstRun: false,
    isUpdated,
    updateType,
    oldVersion: oldVersion2,
    newVersion,
  };
}

/**
 * Save the current version2 to preferences.
 * Call this after initialization to record the current version.
 */
export function saveCurrentVersion(): void {
  const currentVersion = NoranekoConstants.version2 as string;
  Services.prefs.setStringPref(PREF_OLD_VERSION2, currentVersion);
}

/**
 * Check for version2 updates and trigger Firefox update if needed.
 * This will invoke the native Firefox update mechanism.
 */
export async function triggerUpdateIfNeeded(): Promise<{
  triggered: boolean;
  status: Version2UpdateStatus;
}> {
  const status = await checkForVersion2Updates();

  if (!status.hasUpdate) {
    console.log("[NoranekoUpdateChecker] No version2 update available");
    return { triggered: false, status };
  }

  console.log(
    `[NoranekoUpdateChecker] Version2 update detected: ${status.localVersion2} -> ${status.remoteVersion2}`,
  );

  try {
    // Get the update service
    const aus = Cc["@mozilla.org/updates/update-service;1"].getService(
      Ci.nsIApplicationUpdateService,
    );

    // Check if updates are enabled
    if (!aus.canCheckForUpdates) {
      console.warn("[NoranekoUpdateChecker] Update checks are disabled");
      return { triggered: false, status };
    }

    // Manual update triggering
    // Since nsIUpdateChecker might ignore our version2 update logic,
    // we manually construct the update object and feed it to the service.
    // We need to re-fetch to get full details for the update object
    const remoteInfo = await fetchRemoteUpdateInfo();
    if (!remoteInfo) {
      console.error(
        "[NoranekoUpdateChecker] Failed to re-fetch update info for execution",
      );
      return { triggered: false, status };
    }

    const update = new NoranekoUpdate(remoteInfo);
    console.log(
      "[NoranekoUpdateChecker] Manually triggering update download...",
      update,
    );

    // false = background download (not foreground check)
    // Actually, to mimic "Check for updates" behavior, we might want to just start downloading
    // or notify widely. downloadUpdate() is the standard way to start the process once update is found.
    aus.downloadUpdate(update);

    return { triggered: true, status };
  } catch (error) {
    console.error("[NoranekoUpdateChecker] Failed to trigger update:", error);
    return { triggered: false, status };
  }
}
