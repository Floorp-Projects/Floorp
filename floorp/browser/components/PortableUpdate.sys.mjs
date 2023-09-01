/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const EXPORTED_SYMBOLS = [];

import { ExtensionParent } from "resource://gre/modules/ExtensionParent.sys.mjs"
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs"
import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs"

// Migration from JSM to ES Module in the future.
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const L10N = new Localization(["browser/floorp.ftl"]);
const ZipReader = Components.Constructor(
    "@mozilla.org/libjar/zip-reader;1",
    "nsIZipReader",
    "open"
);
const AlertsService = Cc["@mozilla.org/alerts-service;1"].getService(
    Ci.nsIAlertsService
);

const API_BASE_URL = "https://floorp-update.ablaze.one";

const platformInfo = ExtensionParent.PlatformInfo;
const isWin = platformInfo.os === "win";
const displayVersion = AppConstants.MOZ_APP_VERSION_DISPLAY;

const appDirPath = Services.dirsvc.get("XREExeF", Ci.nsIFile).parent.path;
const appDirParentDirPath = PathUtils.parent(appDirPath);
const updateTmpDirPath = PathUtils.join(appDirParentDirPath, "update_tmp");
const updateZipFilePath = PathUtils.join(updateTmpDirPath, "update.zip");
const coreUpdateReadyFilePath = PathUtils.join(updateTmpDirPath, "CORE_UPDATE_READY");
const redirectorUpdateReadyFilePath = PathUtils.join(updateTmpDirPath, "REDIRECTOR_UPDATE_READY");

const portableUpdateUtils = {
    async checkUpdate() {
        let url = `${API_BASE_URL}/browser-portable/latest.json`;
        let data = await new Promise((resolve, reject) => {
            fetch(url)
                .then(res => {
                    if (res.status !== 200){
                        throw new Error`${res.status} ${res.statusText}`;
                    }
                    return res.json();
                })
                .then(data => {
                    let platformKeyName = 
                        platformInfo.os === "mac" ?
                            "mac" :
                            `${platformInfo.os}-${platformInfo.arch}`;
                    resolve(data[platformKeyName]);
                })
                .catch(reject);
        });
        let isUpdateFound = data.version !== displayVersion && Services.prefs.getBoolPref("floorp.portable.isUpdate");
        return {
            isLatest: !isUpdateFound,
            url: isUpdateFound ?
                    data.url :
                    null
        }
    },
    async applyRedirectorUpdate() {
        if (await IOUtils.exists(coreUpdateReadyFilePath)) {
            return { success: false, fatal: false, reason: "coreUpdateNotApplied" };
        }

        if (await IOUtils.exists(redirectorUpdateReadyFilePath)) {
            try {
                await IOUtils.remove(
                    isWin ?
                        PathUtils.join(appDirParentDirPath, "floorp.exe") :
                        PathUtils.join(appDirParentDirPath, "floorp")
                );
            } catch (e) {
                console.error(e);
                return { success: false, fatal: true, reason: "failedRemoveOldFloorp" };
            }

            try {
                await IOUtils.move(
                    isWin ?
                        PathUtils.join(updateTmpDirPath, "floorp.exe") :
                        PathUtils.join(updateTmpDirPath, "floorp"),
                    isWin ?
                        PathUtils.join(appDirParentDirPath, "floorp.exe") :
                        PathUtils.join(appDirParentDirPath, "floorp")
                );
            } catch (e) {
                console.error(e);
                return { success: false, fatal: true, reason: "failedMoveNewFloorp" };
            }

            try {
                await IOUtils.remove(redirectorUpdateReadyFilePath);
            } catch (e) {
                console.error(e);
                return { success: false, fatal: true, reason: "failedRemoveReadyFile" };
            }
        } else {
            return { success: false, fatal: false, reason: "noUpdatesFound" };
        }
        return { success: true, fatal: false, reason: "" };
    },
    async downloadUpdate(url) {
        let data = await new Promise((resolve, reject) => {
            fetch(url)
                .then(res => {
                    if (res.status !== 200){
                        throw new Error`${res.status} ${res.statusText}`;
                    }
                    return res.arrayBuffer();
                })
                .then(resolve)
                .catch(reject);
        });
        await IOUtils.write(updateZipFilePath, new Uint8Array(data));

        let zipreader = new ZipReader(
            FileUtils.File(updateZipFilePath)
        );
        let entries = [];
        for (let entry of zipreader.findEntries("*")) {
            entries.push(entry);
        }
        entries.sort((entry1, entry2) => {
            return String(entry1).length - String(entry2).length
        });
        for (let entry of entries) {
            let entryPath =
                isWin ?
                    String(entry).replaceAll("/", "\\") :
                    String(entry);
            try {
                // Example errors
                // PathUtils.splitRelative: PathUtils.splitRelative: Empty directory components ("") not allowed by options
                // PathUtils.splitRelative: PathUtils.splitRelative: Parent directory components ("..") not allowed by options
                // PathUtils.splitRelative: PathUtils.splitRelative requires a relative path
                PathUtils.splitRelative(
                    entryPath,
                    { allowEmpty: false, allowCurrentDir: false, allowParentDir: false }
                );
            } catch (e) {
                throw console.error("!!! Zip Slip detected !!!");
            }
            let path = PathUtils.joinRelative(
                updateTmpDirPath,
                entryPath
            );
            await zipreader.extract(
                entry,
                FileUtils.File(path)
            )
        }
        zipreader.close();

        await IOUtils.writeUTF8(coreUpdateReadyFilePath, "");
    },
};

(async () => {
    let result = await portableUpdateUtils.applyRedirectorUpdate();
    if (result.success) {
        AlertsService.showAlertNotification(
            "chrome://browser/skin/updater/link-48-last.png", // Image URL
            await L10N.formatValue("update-portable-notification-success-title"), // Title
            await L10N.formatValue("update-portable-notification-success-message"), // Body
            true, // textClickable
            null, // id
            null // clickCallback
        );
    } else if (!result.success && result.fatal) {
        console.error(result);
        AlertsService.showAlertNotification(
            "chrome://browser/skin/updater/failed.png",
            await L10N.formatValue("update-portable-notification-failed-title"),
            await L10N.formatValue("update-portable-notification-failed-redirector-message"),
            true,
            null,
            null
        );
        return;
    } else {
        // no fatal error
    }

    if (await IOUtils.exists(updateTmpDirPath)) {
        await IOUtils.remove(updateTmpDirPath, { recursive: true });
    }

    let updateInfo = await portableUpdateUtils.checkUpdate();
    if (!updateInfo.isLatest) {
        console.log("Update found.");
        AlertsService.showAlertNotification(
            "chrome://browser/skin/updater/link-48.png",
            await L10N.formatValue("update-portable-notification-found-title"),
            await L10N.formatValue("update-portable-notification-found-message"),
            true,
            null,
            null
        );

        try {
            await portableUpdateUtils.downloadUpdate(updateInfo.url);
        } catch (e) {
            console.error(e);
            AlertsService.showAlertNotification(
                "chrome://browser/skin/updater/failed.png",
                await L10N.formatValue("update-portable-notification-failed-title"),
                await L10N.formatValue("update-portable-notification-failed-prepare-message"),
                true,
                null,
                null
            );
            return;
        }

        AlertsService.showAlertNotification(
            "chrome://browser/skin/updater/link-48.png",
            await L10N.formatValue("update-portable-notification-ready-title"),
            await L10N.formatValue("update-portable-notification-ready-message"),
            true,
            null,
            null
        );
    }
})();
