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

    let encryptionEnabled = document.querySelector("#encryption-enabled");
    encryptionEnabled.addEventListener("click", this);

    // We use `init` instead of `get` here, since this page might load before
    // the BackupService has had a chance to initialize itself.
    let service = BackupService.init();
    service.addEventListener("BackupService:StateUpdate", this);
    this.onStateUpdate();

    // Kick-off reading any pre-existing encryption state off of the disk.
    service.loadEncryptionState();
  },

  handleEvent(event) {
    switch (event.type) {
      case "BackupService:StateUpdate": {
        this.onStateUpdate();
        break;
      }
      case "click": {
        let target = event.target;
        if (HTMLButtonElement.isInstance(event.target)) {
          this.onButtonClick(target);
        } else if (
          HTMLInputElement.isInstance(event.target) &&
          event.target.type == "checkbox"
        ) {
          event.preventDefault();
          this.onCheckboxClick(target);
        }
        break;
      }
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
        let backupsDir = PathUtils.join(
          PathUtils.profileDir,
          BackupService.PROFILE_FOLDER_NAME
        );

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
        let backupsDir = PathUtils.join(
          PathUtils.profileDir,
          BackupService.PROFILE_FOLDER_NAME
        );
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
        break;
      }
    }
  },

  async onCheckboxClick(checkbox) {
    if (checkbox.id == "encryption-enabled") {
      let service = BackupService.get();
      if (checkbox.checked) {
        let password = prompt("What's the encryption password? (8 char min)");
        if (password != null) {
          try {
            await service.enableEncryption(password);
          } catch (e) {
            console.error(e);
          }
        }
      } else if (confirm("Disable encryption?")) {
        try {
          await service.disableEncryption();
        } catch (e) {
          console.error(e);
        }
      }
    }
  },

  onStateUpdate() {
    let service = BackupService.get();
    let state = service.state;

    let encryptionEnabled = document.querySelector("#encryption-enabled");
    encryptionEnabled.checked = state.encryptionEnabled;
  },
};

// Wait until the load event fires before setting up any listeners or updating
// any of the state of the page. We do this in order to avoid having any of
// our control states overwritten by SessionStore after a restoration, as
// restoration of form state occurs _prior_ to the load event firing.
addEventListener(
  "load",
  () => {
    DebugUI.init();
  },
  { once: true }
);
