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

  async onButtonClick(button) {
    switch (button.id) {
      case "create-backup": {
        let service = BackupService.get();
        button.disabled = true;
        await service.createBackup();
        button.disabled = false;
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
    }
  },
};

DebugUI.init();
