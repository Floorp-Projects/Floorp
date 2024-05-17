/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/turn-on-scheduled-backups.mjs";

/**
 * The widget for managing the BackupService that is embedded within the main
 * document of about:settings / about:preferences.
 */
export default class BackupSettings extends MozLitElement {
  static properties = {
    backupServiceState: { type: Object },
  };

  static get queries() {
    return {
      scheduledBackupsButtonEl: "#backup-toggle-scheduled-button",
      turnOnScheduledBackupsDialogEl: "#turn-on-scheduled-backups-dialog",
      turnOnScheduledBackupsEl: "turn-on-scheduled-backups",
    };
  }

  /**
   * Creates a BackupPreferences instance and sets the initial default
   * state.
   */
  constructor() {
    super();
    this.backupServiceState = {
      backupFilePath: "Documents", // TODO: make save location configurable (bug 1895943)
      backupInProgress: false,
      scheduledBackupsEnabled: false,
    };
  }

  /**
   * Dispatches the BackupUI:InitWidget custom event upon being attached to the
   * DOM, which registers with BackupUIChild for BackupService state updates.
   */
  connectedCallback() {
    super.connectedCallback();
    this.dispatchEvent(
      new CustomEvent("BackupUI:InitWidget", { bubbles: true })
    );

    this.addEventListener("scheduledBackupsCancel", this);
    this.addEventListener("scheduledBackupsConfirm", this);
  }

  handleEvent(event) {
    switch (event.type) {
      case "scheduledBackupsConfirm":
        this.turnOnScheduledBackupsDialogEl.close();
        this.dispatchEvent(
          new CustomEvent("BackupUI:ScheduledBackupsConfirm", {
            bubbles: true,
            composed: true,
          })
        );
        break;
      case "scheduledBackupsCancel":
        this.turnOnScheduledBackupsDialogEl.close();
        break;
    }
  }

  handleShowScheduledBackups() {
    if (
      !this.backupServiceState.scheduledBackupsEnabled &&
      this.turnOnScheduledBackupsDialogEl
    ) {
      this.turnOnScheduledBackupsDialogEl.showModal();
    }
  }

  turnOnScheduledBackupsDialogTemplate() {
    return html`<dialog id="turn-on-scheduled-backups-dialog">
      <turn-on-scheduled-backups
        .backupFilePath=${this.backupServiceState.backupFilePath}
      ></turn-on-scheduled-backups>
    </dialog>`;
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/skin/preferences/preferences.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/backup-settings.css"
      />
      <div id="scheduled-backups">
        <div>
          Backup in progress:
          ${this.backupServiceState.backupInProgress ? "Yes" : "No"}
        </div>

        ${this.turnOnScheduledBackupsDialogTemplate()}

        <moz-button
          id="backup-toggle-scheduled-button"
          @click=${this.handleShowScheduledBackups}
          data-l10n-id="settings-data-backup-toggle"
        ></moz-button>
      </div>`;
  }
}

customElements.define("backup-settings", BackupSettings);
