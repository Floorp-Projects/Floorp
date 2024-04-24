/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

Preferences.addAll([
  { id: "browser.backup.enabled", type: "bool" },
  { id: "browser.backup.log", type: "bool" },
]);

const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);

let DebugUI = {
  init() {
    let controls = document.querySelector("#controls");
    controls.addEventListener("click", this);
  },

  handleEvent(event) {
    let target = event.target;
    if (HTMLButtonElement.isInstance(event.target)) {
      this.onButtonClick(target);
    }
  },

  secondsToHms(seconds) {
    let h = Math.floor(seconds / 3600);
    let m = Math.floor((seconds % 3600) / 60);
    let s = Math.floor((seconds % 3600) % 60);
    return `${h}h ${m}m ${s}s`;
  },

  async onButtonClick(button) {
    switch (button.id) {
      case "create-backup": {
        let service = BackupService.get();
        let lastBackupStatus = document.querySelector("#last-backup-status");
        lastBackupStatus.textContent = "Creating backup...";

        let then = Cu.now();
        button.disabled = true;
        await service.createBackup();
        let totalTimeSeconds = (Cu.now() - then) / 1000;
        button.disabled = false;
        new Notification(`Backup created`, {
          body: `Total time ${this.secondsToHms(totalTimeSeconds)}`,
        });
        lastBackupStatus.textContent = `Backup created - total time: ${this.secondsToHms(
          totalTimeSeconds
        )}`;
        break;
      }
      case "open-backup-folder": {
        let backupsDir = PathUtils.join(PathUtils.profileDir, "backups");

        let nsLocalFile = Components.Constructor(
          "@mozilla.org/file/local;1",
          "nsIFile",
          "initWithPath"
        );

        if (await IOUtils.exists(backupsDir)) {
          new nsLocalFile(backupsDir).reveal();
        } else {
          alert("backups folder doesn't exist yet");
        }

        break;
      }
      case "recover-from-staging": {
        let backupsDir = PathUtils.join(PathUtils.profileDir, "backups");
        let fp = Cc["@mozilla.org/filepicker;1"].createInstance(
          Ci.nsIFilePicker
        );
        fp.init(
          window.browsingContext,
          "Choose a staging folder",
          Ci.nsIFilePicker.modeGetFolder
        );
        fp.displayDirectory = await IOUtils.getDirectory(backupsDir);
        let result = await new Promise(resolve => fp.open(resolve));
        if (result == Ci.nsIFilePicker.returnCancel) {
          break;
        }

        let path = fp.file.path;
        let lastRecoveryStatus = document.querySelector(
          "#last-recovery-status"
        );
        lastRecoveryStatus.textContent = "Recovering from backup...";

        let service = BackupService.get();
        try {
          let newProfile = await service.recoverFromBackup(
            path,
            true /* shouldLaunch */
          );
          lastRecoveryStatus.textContent = `Created profile ${newProfile.name} at ${newProfile.rootDir.path}`;
        } catch (e) {
          lastRecoveryStatus.textContent(
            `Failed to recover: ${e.message} Check the console for the full exception.`
          );
          throw e;
        }
      }
    }
  },
};

DebugUI.init();
