(async() => {
    const API_BASE_URL = "https://floorp-update.ablaze.one";

    let platformInfo = await browser.runtime.getPlatformInfo();
    let isWin = platformInfo["os"] === "win";

    async function getL10nValue(id) {
        return (await browser.browserL10n.getFloorpL10nValues({ file: ["browser/floorp.ftl"], text: [id] }))[0]
    }

    async function checkUpdate() {
        let displayVersion = await browser.BrowserInfo.getDisplayVersion();
        let url = `${API_BASE_URL}/browser-portable/latest.json`;
        let data = await new Promise((resolve, reject) => {
            fetch(url)
                .then(res => {
                    if (res.status !== 200) throw `${res.status} ${res.statusText}`;
                    return res.json();
                })
                .then(datas => {
                    let platformKeyName = 
                        platformInfo["os"] === "mac" ?
                            "mac" :
                            `${platformInfo["os"]}-${platformInfo["arch"]}`;
                    let data = datas[platformKeyName];
                    resolve(data);
                })
                .catch(reject);
        });
        let isUpdateFound = data["version"] !== displayVersion;
        return {
            isLatest: !isUpdateFound,
            url: isUpdateFound ?
                    data["url"] :
                    null
        }
    }

    let main = async () => {
        let isPortable = await browser.aboutConfigPrefs.getBoolPref("floorp.isPortable");
        console.log(`floorp.isPortable: ${isPortable}`);
        if (!isPortable) return;

        let appdir = await browser.BrowserInfo.getAppExecutableDirPath();
        let appdir_parent =
            appdir.split(isWin ? "\\" : "/")
            .slice(0, -1)
            .join(isWin ? "\\" : "/");
        let updateTmpDirPath =
            isWin ?
                appdir_parent + "\\update_tmp" :
                appdir_parent + "/update_tmp";
        let updateZipPath =
            isWin ?
                updateTmpDirPath + "\\update.zip" :
                updateTmpDirPath + "/update.zip";
        let coreUpdateReadyFilePath =
            isWin ?
                updateTmpDirPath + "\\CORE_UPDATE_READY" :
                updateTmpDirPath + "/CORE_UPDATE_READY"
        let redirectorUpdateReadyFilePath =
            isWin ?
                updateTmpDirPath + "\\REDIRECTOR_UPDATE_READY" :
                updateTmpDirPath + "/REDIRECTOR_UPDATE_READY"

        if (await browser.IOFile.exists(coreUpdateReadyFilePath)) {
            return;
        }

        if (await browser.IOFile.exists(redirectorUpdateReadyFilePath)) {
            try {
                await browser.IOFile.removeFile(
                    isWin ?
                        appdir_parent + "\\floorp.exe" :
                        appdir_parent + "/floorp"
                );
                await browser.IOFile.move(
                    isWin ?
                        updateTmpDirPath + "\\floorp.exe" :
                        updateTmpDirPath + "/floorp",
                    isWin ?
                        appdir_parent + "\\floorp.exe" :
                        appdir_parent + "/floorp"
                );
                await browser.IOFile.removeFile(redirectorUpdateReadyFilePath);

                browser.notifications.create({
                    "type": "basic",
                    "iconUrl": browser.runtime.getURL("icons/link-48-last.png"),
                    "title": (await getL10nValue("update-portable-notification-success-title")),
                    "message": (await getL10nValue("update-portable-notification-success-message"))
                });
            } catch (e) {
                console.error(e);
                browser.notifications.create({
                    "type": "basic",
                    "iconUrl": browser.runtime.getURL("icons/failed.png"),
                    "title": (await getL10nValue("update-portable-notification-failed-title")),
                    "message": (await getL10nValue("update-portable-notification-failed-redirector-message"))
                });
            }
        }

        if (await browser.IOFile.exists(updateTmpDirPath)) {
            await browser.IOFile.removeDir(updateTmpDirPath);
        }

        let updateInfo = await checkUpdate();

        if (!updateInfo["isLatest"]) {
            console.log("Updates found.");
            browser.notifications.create({
                "type": "basic",
                "iconUrl": browser.runtime.getURL("icons/link-48.png"),
                "title": (await getL10nValue("update-portable-notification-found-title")),
                "message": (await getL10nValue("update-portable-notification-found-message"))
            });

            try {
                await browser.downloadFile.download(
                    updateInfo["url"],
                    updateZipPath
                );
    
                await browser.decompressZip.decompress(
                    updateZipPath,
                    updateTmpDirPath
                );
    
                await browser.IOFile.createEmptyFile(coreUpdateReadyFilePath);
            } catch (e) {
                console.error(e);
                browser.notifications.create({
                    "type": "basic",
                    "iconUrl": browser.runtime.getURL("icons/failed.png"),
                    "title": (await getL10nValue("update-portable-notification-failed-title")),
                    "message": (await getL10nValue("update-portable-notification-failed-prepare-message"))
                });
            }
        } else {
            console.log("No updates found.");
        }
    };
    main();
    setInterval(main, 1000 * 60 * 60 * 12);
})();
