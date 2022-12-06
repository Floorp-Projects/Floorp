/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component contains the UI that steps users through migrating their
 * data from other browsers to this one. This component only contains very
 * basic logic and structure for the UI, and most of the state management
 * occurs in the MigrationWizardChild JSWindowActor.
 */
export class MigrationWizard extends HTMLElement {
  static #template = null;
  static #PAGES = Object.freeze({
    SELECTION: "selection",
    PROGRESS: "progress",
    SAFARI_PERMISSION: "safari-permission",
  });

  #deck = null;
  #browserProfileSelector = null;

  static get markup() {
    return `
      <template>
        <link rel="stylesheet" href="chrome://browser/skin/migration/migration-wizard.css">
        <named-deck id="wizard-deck" selected-view="page-selection">

          <div name="page-selection">
            <h3 data-l10n-id="migration-wizard-header"></h3>
            <select id="browser-profile-selector">
            </select>
            <fieldset>
              <label for="bookmarks">
                <input type="checkbox" id="bookmarks"/><span data-l10n-id="migration-bookmarks-option-label"></span>
              </label>
              <label for="logins-and-passwords">
                <input type="checkbox" id="logins-and-passwords"/><span data-l10n-id="migration-logins-and-passwords-option-label"></span>
              </label>
              <label for="history">
                <input type="checkbox" id="history"/><span data-l10n-id="migration-history-option-label"></span>
              </label>
              <label for="form-autofill">
                <input type="checkbox" id="form-autofill"/><span data-l10n-id="migration-form-autofill-option-label"></span>
              </label>
            </fieldset>
            <div class="buttons">
              <button data-l10n-id="migration-cancel-button-label"></button>
              <button class="primary" data-l10n-id="migration-import-button-label"></button>
            </div>
          </div>

          <div name="page-progress">
            <h3>TODO: Progress page</h3>
          </div>

          <div name="page-safari-permission">
            <h3>TODO: Safari permission page</h3>
          </div>
        </named-deck>
      </template>
    `;
  }

  static get fragment() {
    if (!MigrationWizard.#template) {
      let parser = new DOMParser();
      let doc = parser.parseFromString(MigrationWizard.markup, "text/html");
      MigrationWizard.#template = document.importNode(
        doc.querySelector("template"),
        true
      );
    }
    let fragment = MigrationWizard.#template.content.cloneNode(true);
    if (window.IS_STORYBOOK) {
      // If we're using Storybook, load the CSS from the static local file
      // system rather than chrome:// to take advantage of auto-reloading.
      fragment.querySelector("link[rel=stylesheet]").href =
        "/migration/migration-wizard.css";
    }
    return fragment;
  }

  static get PAGES() {
    return this.#PAGES;
  }

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "closed" });

    if (window.MozXULElement) {
      window.MozXULElement.insertFTLIfNeeded(
        "locales-preview/migrationWizard.ftl"
      );
    }
    document.l10n.connectRoot(shadow);

    shadow.appendChild(MigrationWizard.fragment);

    this.#deck = shadow.querySelector("#wizard-deck");
    this.#browserProfileSelector = shadow.querySelector(
      "#browser-profile-selector"
    );
  }

  connectedCallback() {
    this.dispatchEvent(
      new CustomEvent("MigrationWizard:Init", { bubbles: true })
    );
  }

  /**
   * This is the main entrypoint for updating the state and appearance of
   * the wizard.
   *
   * @param {object} state The state to be represented by the component.
   * @param {string} state.page The page of the wizard to display. This should
   *   be one of the MigrationWizard.PAGES constants.
   */
  setState(state) {
    if (state.page == MigrationWizard.PAGES.SELECTION) {
      this.#onShowingSelection(state);
    }

    this.#deck.setAttribute("selected-view", `page-${state.page}`);
  }

  /**
   * Called when showing the browser/profile selection page of the wizard.
   *
   * @param {object} state
   *   The state object passed into setState. The following properties are
   *   used:
   * @param {string[]} state.migrators An array of source browser names that
   *   can be migrated from.
   */
  #onShowingSelection(state) {
    this.#browserProfileSelector.textContent = "";

    for (let migratorKey of state.migrators) {
      let opt = document.createElement("option");
      opt.value = migratorKey;
      opt.textContent = migratorKey;
      this.#browserProfileSelector.appendChild(opt);
    }
  }
}

if (globalThis.customElements) {
  customElements.define("migration-wizard", MigrationWizard);
}
