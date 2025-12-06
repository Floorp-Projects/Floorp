/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const { NoranekoConstants } = ChromeUtils.importESModule(
  "resource://noraneko/modules/NoranekoConstants.sys.mjs",
);

const PREF_OLD_VERSION2 = "floorp.startup.oldVersion2";

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
 * Determine the type of update based on version comparison.
 */
function getUpdateType(oldVersion: string, newVersion: string): UpdateType {
  const oldParsed = parseVersion(oldVersion);
  const newParsed = parseVersion(newVersion);

  if (!oldParsed || !newParsed) return null;

  if (newParsed.major !== oldParsed.major) {
    return "major";
  }
  if (newParsed.minor !== oldParsed.minor) {
    return "minor";
  }
  if (newParsed.patch !== oldParsed.patch) {
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
  // FOREGROUND_CHECK = 2
  const url = await checker.getUpdateURL(2);
  console.log(`[NoranekoUpdateChecker] Using nsIUpdateChecker URL: ${url}`);
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
    };
  } catch (error) {
    console.error("[NoranekoUpdateChecker] Error parsing update.xml:", error);
    return null;
  }
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

  // Compare versions
  const hasUpdate =
    localVersion2 !== remoteVersion2 || localBuildID2 !== remoteBuildID2;
  const updateType = hasUpdate
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
 */
export function checkForUpdates(): UpdateInfo {
  const newVersion = NoranekoConstants.version2 as string;

  // Check if this is the first run (no old version stored)
  const oldVersion = Services.prefs.getStringPref(PREF_OLD_VERSION2, "");
  const isFirstRun = oldVersion === "";

  if (isFirstRun) {
    return {
      isFirstRun: true,
      isUpdated: false,
      updateType: null,
      oldVersion: null,
      newVersion,
    };
  }

  // Compare versions
  const isUpdated = oldVersion !== newVersion;
  const updateType = isUpdated ? getUpdateType(oldVersion, newVersion) : null;

  return {
    isFirstRun: false,
    isUpdated,
    updateType,
    oldVersion,
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

    // Trigger a foreground update check
    // FOREGROUND_CHECK = 1
    const checker = Cc["@mozilla.org/updates/update-checker;1"].getService(
      Ci.nsIUpdateChecker,
    );
    checker.checkForUpdates(1); // FOREGROUND_CHECK

    console.log("[NoranekoUpdateChecker] Firefox update check triggered");
    return { triggered: true, status };
  } catch (error) {
    console.error("[NoranekoUpdateChecker] Failed to trigger update:", error);
    return { triggered: false, status };
  }
}
