/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * The widget for showing available options when users want to turn on
 * scheduled backups.
 */
export default class TurnOnScheduledBackups extends MozLitElement {
  static properties = {
    backupFilePath: { type: String },
    showPasswordOptions: { type: Boolean, reflect: true },
  };

  static get queries() {
    return {
      cancelButtonEl: "#backup-turn-on-scheduled-cancel-button",
      confirmButtonEl: "#backup-turn-on-scheduled-confirm-button",
      passwordOptionsCheckboxEl: "#sensitive-data-checkbox-input",
      passwordOptionsExpandedEl: "#passwords",
      recommendedFolderInputEl: "#backup-location-filepicker-input",
    };
  }

  constructor() {
    super();
    this.backupFilePath = null;
    this.showPasswordOptions = false;
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
  }

  handleChooseLocation() {
    // TODO: show file picker (bug 1895943)
  }

  handleCancel() {
    this.dispatchEvent(
      new CustomEvent("scheduledBackupsCancel", {
        bubbles: true,
        composed: true,
      })
    );
    this.showPasswordOptions = false;
    this.passwordOptionsCheckboxEl.checked = false;
  }

  handleConfirm() {
    /**
     * TODO:
     * We should pass save location to BackupUIParent here (bug 1895943).
     * If encryption is enabled via this dialog, ensure a password is set and pass it to BackupUIParent (bug 1895981).
     * Before confirmation, verify passwords match and FxA format rules (bug 1896772).
     */
    this.dispatchEvent(
      new CustomEvent("scheduledBackupsConfirm", {
        bubbles: true,
        composed: true,
      })
    );
    this.showPasswordOptions = false;
    this.passwordOptionsCheckboxEl.checked = false;
  }

  handleTogglePasswordOptions() {
    this.showPasswordOptions = this.passwordOptionsCheckboxEl?.checked;
  }

  allOptionsTemplate() {
    return html`
      <fieldset id="all-controls">
        <fieldset id="backup-location-controls">
          <label
            id="backup-location-label"
            for="backup-location-filepicker-input"
            data-l10n-id="turn-on-scheduled-backups-location-label"
          ></label>
          <!-- TODO: show folder icon (bug 1895943) -->
          <div id="backup-location-filepicker">
            <input
              id="backup-location-filepicker-input"
              type="text"
              readonly
              data-l10n-id="turn-on-scheduled-backups-location-default-folder"
              data-l10n-args=${JSON.stringify({
                recommendedFolder: this.backupFilePath,
              })}
              data-l10n-attrs="value"
            />
            <moz-button
              id="backup-location-filepicker-button"
              @click=${this.handleChooseLocation}
              data-l10n-id="turn-on-scheduled-backups-location-choose-button"
              aria-controls="backup-location-filepicker-input"
            ></moz-button>
          </div>
        </fieldset>

        <fieldset id="sensitive-data-controls">
          <div id="sensitive-data-checkbox">
            <label
              id="sensitive-data-checkbox-label"
              for="sensitive-data-checkbox-input"
              aria-controls="passwords"
              aria-expanded=${this.showPasswordOptions}
            >
              <input
                id="sensitive-data-checkbox-input"
                value=${this.showPasswordOptions}
                @click=${this.handleTogglePasswordOptions}
                type="checkbox"
              />
              <span
                id="sensitive-data-checkbox-span"
                data-l10n-id="turn-on-scheduled-backups-encryption-label"
              ></span>
            </label>
            <span
              class="text-deemphasized"
              data-l10n-id="turn-on-scheduled-backups-encryption-description"
            ></span>
          </div>

          ${this.showPasswordOptions ? this.passwordOptionsTemplate() : null}
        </fieldset>
      </fieldset>
    `;
  }

  passwordOptionsTemplate() {
    return html`
    <fieldset id="passwords">
      <label id="new-password-label" for="new-password-input">
        <span id="new-password-span" data-l10n-id="turn-on-scheduled-backups-encryption-create-password-label"></span>
        <input type="password" id="new-password-input"/>
    </label>
      <label id="repeat-password-label" for="repeat-password-input">
        <span id="repeat-password-span" data-l10n-id="turn-on-scheduled-backups-encryption-repeat-password-label"></span>
        <input type="password" id="repeat-password-input"/>
      </label>
    </fieldset>
  </fieldset>`;
  }

  contentTemplate() {
    return html`
      <div
        id="backup-turn-on-scheduled-wrapper"
        aria-labelledby="backup-turn-on-scheduled-header"
        aria-describedby="backup-turn-on-scheduled-description"
      >
        <h1
          id="backup-turn-on-scheduled-header"
          data-l10n-id="turn-on-scheduled-backups-header"
        ></h1>
        <main id="backup-turn-on-scheduled-content">
          <div id="backup-turn-on-scheduled-description">
            <span
              id="backup-turn-on-scheduled-description-span"
              data-l10n-id="turn-on-scheduled-backups-description"
            ></span>
            <a
              id="backup-turn-on-scheduled-learn-more-link"
              is="moz-support-link"
              support-page="todo-backup"
              data-l10n-id="turn-on-scheduled-backups-support-link"
            ></a>
          </div>
          ${this.allOptionsTemplate()}
        </main>

        <moz-button-group id="backup-turn-on-scheduled-button-group">
          <moz-button
            id="backup-turn-on-scheduled-cancel-button"
            @click=${this.handleCancel}
            data-l10n-id="turn-on-scheduled-backups-cancel-button"
          ></moz-button>
          <moz-button
            id="backup-turn-on-scheduled-confirm-button"
            @click=${this.handleConfirm}
            type="primary"
            data-l10n-id="turn-on-scheduled-backups-confirm-button"
          ></moz-button>
        </moz-button-group>
      </div>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/turn-on-scheduled-backups.css"
      />
      ${this.contentTemplate()}
    `;
  }
}

customElements.define("turn-on-scheduled-backups", TurnOnScheduledBackups);
