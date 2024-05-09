/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * The widget for managing the BackupService that is embedded within the main
 * document of about:settings / about:preferences.
 */
export default class BackupSettings extends MozLitElement {
  static properties = {
    backupServiceState: { type: Object },
  };

  /**
   * Creates a BackupSettings instance and sets the initial default
   * state.
   */
  constructor() {
    super();
    this.backupServiceState = {
      backupInProgress: false,
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
  }

  render() {
    return html`<div>
      Backup in progress:
      ${this.backupServiceState.backupInProgress ? "Yes" : "No"}
    </div>`;
  }
}

customElements.define("backup-settings", BackupSettings);
