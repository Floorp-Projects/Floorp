/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button-group.mjs";
import { MigrationWizardConstants } from "chrome://browser/content/migration/migration-wizard-constants.mjs";

/**
 * This component contains the UI that steps users through migrating their
 * data from other browsers to this one. This component only contains very
 * basic logic and structure for the UI, and most of the state management
 * occurs in the MigrationWizardChild JSWindowActor.
 */
export class MigrationWizard extends HTMLElement {
  static #template = null;

  #deck = null;
  #browserProfileSelector = null;
  #browserProfileSelectorList = null;
  #resourceTypeList = null;
  #shadowRoot = null;
  #importButton = null;
  #safariPermissionButton = null;
  #selectAllCheckbox = null;

  static get markup() {
    return `
      <template>
        <link rel="stylesheet" href="chrome://browser/skin/migration/migration-wizard.css">
        <named-deck id="wizard-deck" selected-view="page-loading" aria-live="polite" aria-busy="true">
          <div name="page-loading">
            <h1 data-l10n-id="migration-wizard-selection-header"></h1>
            <div class="loading-block large"></div>
            <div class="loading-block small"></div>
            <div class="loading-block small"></div>
            <moz-button-group class="buttons">
              <!-- If possible, use the same button labels as the SELECTION page with the same strings.
                   That'll prevent flicker when the load state exits if we then enter the SELECTION page. -->
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label" disabled></button>
              <button data-l10n-id="migration-import-button-label" disabled></button>
            </moz-button-group>
          </div>

          <div name="page-selection">
            <h1 data-l10n-id="migration-wizard-selection-header"></h1>
            <button id="browser-profile-selector">
              <span class="migrator-icon"></span>
              <div class="migrator-description">
                <div class="migrator-name">&nbsp;</div>
                <div class="profile-name deemphasized-text"></div>
              </div>
              <span class="dropdown-icon"></span>
            </button>
            <div data-l10n-id="migration-wizard-selection-list" class="resource-selection-preamble deemphasized-text"></div>
            <details class="resource-selection-details">
              <summary>
                <div class="selected-data-header" data-l10n-id="migration-all-available-data-label"></div>
                <div class="selected-data deemphasized-text">&nbsp;</div>
                <span class="expand-collapse-icon" role="img"></span>
              </summary>
              <fieldset id="resource-type-list">
                <label id="select-all">
                  <input type="checkbox" class="select-all-checkbox"/><span data-l10n-id="migration-select-all-option-label"></span>
                </label>
                <label id="bookmarks" data-resource-type="BOOKMARKS"/>
                  <input type="checkbox"/><span data-l10n-id="migration-bookmarks-option-label"></span>
                </label>
                <label id="logins-and-passwords" data-resource-type="PASSWORDS">
                  <input type="checkbox"/><span data-l10n-id="migration-logins-and-passwords-option-label"></span>
                </label>
                <label id="history" data-resource-type="HISTORY">
                  <input type="checkbox"/><span data-l10n-id="migration-history-option-label"></span>
                </label>
                <label id="form-autofill" data-resource-type="FORMDATA">
                  <input type="checkbox"/><span data-l10n-id="migration-form-autofill-option-label"></span>
                </label>
              </fieldset>
            </details>

            <moz-button-group class="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label"></button>
              <button id="import" class="primary" data-l10n-id="migration-import-button-label"></button>
            </moz-button-group>
          </div>

          <div name="page-progress">
            <h1 id="progress-header" data-l10n-id="migration-wizard-progress-header"></h1>
            <div class="resource-progress">
              <div data-resource-type="BOOKMARKS" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-bookmarks-option-label"></span>
                <span class="success-text deemphasized-text">&nbsp;</span>
              </div>

              <div data-resource-type="PASSWORDS" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-logins-and-passwords-option-label"></span>
                <span class="success-text deemphasized-text">&nbsp;</span>
              </div>

              <div data-resource-type="HISTORY" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-history-option-label"></span>
                <span class="success-text deemphasized-text">&nbsp;</span>
              </div>

              <div data-resource-type="FORMDATA" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-form-autofill-option-label"></span>
                <span class="success-text deemphasized-text">&nbsp;</span>
              </div>
            </div>
            <moz-button-group class="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label" disabled></button>
              <button class="primary" id="done-button" data-l10n-id="migration-done-button-label"></button>
            </moz-button-group>
          </div>

          <div name="page-safari-password-permission">
            <h1 data-l10n-id="migration-safari-password-import-header"></h1>
            <span data-l10n-id="migration-safari-password-import-steps-header"></span>
            <ol>
              <li data-l10n-id="migration-safari-password-import-step1"></li>
              <li data-l10n-id="migration-safari-password-import-step2"><img class="safari-icon-3dots" data-l10n-name="safari-icon-3dots"/></li>
              <li data-l10n-id="migration-safari-password-import-step3"></li>
              <li class="safari-icons-group">
                <span data-l10n-id="migration-safari-password-import-step4"></span>
                <span class="page-portrait-icon"></span>
              </li>
            </ol>
            <moz-button-group class="buttons">
              <button class="cancel-close" data-l10n-id="migration-safari-password-import-skip-button"></button>
              <button class="primary" data-l10n-id="migration-safari-password-import-select-button"></button>
            </moz-button-group>
          </div>

          <div name="page-safari-permission">
            <h1 data-l10n-id="migration-wizard-selection-header"></h1>
            <div data-l10n-id="migration-wizard-safari-permissions-sub-header"></div>
            <ol>
              <li data-l10n-id="migration-wizard-safari-instructions-continue"></li>
              <li data-l10n-id="migration-wizard-safari-instructions-folder"></li>
            </ol>
            <moz-button-group class="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label"></button>
              <button id="safari-request-permissions" class="primary" data-l10n-id="migration-wizard-safari-select-button"></button>
            </moz-button-group>
          </div>

          <div name="page-no-browsers-found">
            <h1 data-l10n-id="migration-wizard-selection-header"></h1>
            <div class="no-browsers-found">
              <span class="error-icon" role="img"></span>
              <div class="no-browsers-found-message" data-l10n-id="migration-wizard-import-browser-no-browsers"></div>
            </div>
            <moz-button-group class="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label"></button>
            </moz-button-group>
          </div>
        </named-deck>
        <slot></slot>
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
        "./migration/migration-wizard.css";
    }
    return fragment;
  }

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "closed" });

    if (window.MozXULElement) {
      window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");
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

    let cancelCloseButtons = shadow.querySelectorAll(".cancel-close");
    for (let button of cancelCloseButtons) {
      button.addEventListener("click", this);
    }

    let doneCloseButtons = shadow.querySelector("#done-button");
    doneCloseButtons.addEventListener("click", this);

    this.#importButton = shadow.querySelector("#import");
    this.#importButton.addEventListener("click", this);

    this.#browserProfileSelector.addEventListener("click", this);
    this.#resourceTypeList = shadow.querySelector("#resource-type-list");
    this.#resourceTypeList.addEventListener("change", this);

    this.#safariPermissionButton = shadow.querySelector(
      "#safari-request-permissions"
    );
    this.#safariPermissionButton.addEventListener("click", this);

    this.#selectAllCheckbox = shadow.querySelector("#select-all").control;

    this.#shadowRoot = shadow;
  }

  connectedCallback() {
    if (this.hasAttribute("auto-request-state")) {
      this.requestState();
    }
  }

  requestState() {
    this.dispatchEvent(
      new CustomEvent("MigrationWizard:RequestState", { bubbles: true })
    );
  }

  /**
   * This setter can be used in the event that the MigrationWizard is being
   * inserted via Lit, and the caller wants to set state declaratively using
   * a property expression.
   *
   * @param {object} state
   *   The state object to pass to setState.
   * @see MigrationWizard.setState.
   */
  set state(state) {
    this.setState(state);
  }

  /**
   * This is the main entrypoint for updating the state and appearance of
   * the wizard.
   *
   * @param {object} state The state to be represented by the component.
   * @param {string} state.page The page of the wizard to display. This should
   *   be one of the MigrationWizardConstants.PAGES constants.
   */
  setState(state) {
    switch (state.page) {
      case MigrationWizardConstants.PAGES.SELECTION: {
        this.#onShowingSelection(state);
        break;
      }
      case MigrationWizardConstants.PAGES.PROGRESS: {
        this.#onShowingProgress(state);
        break;
      }
    }

    this.#deck.toggleAttribute(
      "aria-busy",
      state.page == MigrationWizardConstants.PAGES.LOADING
    );
    this.#deck.setAttribute("selected-view", `page-${state.page}`);

    if (window.IS_STORYBOOK) {
      this.#updateForStorybook();
    }
  }

  #ensureSelectionDropdown() {
    if (this.#browserProfileSelectorList) {
      return;
    }
    this.#browserProfileSelectorList = this.querySelector("panel-list");
    if (!this.#browserProfileSelectorList) {
      throw new Error(
        "Could not find a <panel-list> under the MigrationWizard during initialization."
      );
    }
    this.#browserProfileSelectorList.toggleAttribute(
      "min-width-from-anchor",
      true
    );
    this.#browserProfileSelectorList.addEventListener("click", this);
  }

  /**
   * Reacts to changes to the browser / profile selector dropdown. This
   * should update the list of resource types to match what's supported
   * by the selected migrator and profile.
   *
   *  @param {Element} panelItem the selected <oabel-item>
   */
  #onBrowserProfileSelectionChanged(panelItem) {
    this.#browserProfileSelector.selectedPanelItem = panelItem;
    this.#browserProfileSelector.querySelector(".migrator-name").textContent =
      panelItem.displayName;
    this.#browserProfileSelector.querySelector(".profile-name").textContent =
      panelItem.profile?.name || "";

    if (panelItem.brandImage) {
      this.#browserProfileSelector.querySelector(
        ".migrator-icon"
      ).style.content = `url(${panelItem.brandImage})`;
    } else {
      this.#browserProfileSelector.querySelector(
        ".migrator-icon"
      ).style.content = "url(chrome://global/skin/icons/defaultFavicon.svg)";
    }

    let resourceTypes = panelItem.resourceTypes;
    for (let child of this.#resourceTypeList.children) {
      child.hidden = true;
      child.control.checked = false;
    }

    for (let resourceType of resourceTypes) {
      let resourceLabel = this.#resourceTypeList.querySelector(
        `label[data-resource-type="${resourceType}"]`
      );
      if (resourceLabel) {
        resourceLabel.hidden = false;
        resourceLabel.control.checked = true;
      }
    }
    let selectAll = this.#shadowRoot.querySelector("#select-all").control;
    selectAll.checked = true;
    this.#displaySelectedResources();
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
    this.#ensureSelectionDropdown();
    this.#browserProfileSelectorList.textContent = "";

    let selectionPage = this.#shadowRoot.querySelector(
      "div[name='page-selection']"
    );

    let details = this.#shadowRoot.querySelector("details");
    selectionPage.toggleAttribute("show-import-all", state.showImportAll);
    details.open = !state.showImportAll;

    for (let migrator of state.migrators) {
      let opt = document.createElement("panel-item");
      opt.setAttribute("key", migrator.key);
      opt.profile = migrator.profile;
      opt.displayName = migrator.displayName;
      opt.resourceTypes = migrator.resourceTypes;
      opt.hasPermissions = migrator.hasPermissions;
      opt.brandImage = migrator.brandImage;

      // Bug 1823489 - since the panel-list and panel-items are slotted, we
      // cannot style them directly from migration-wizard.css. We use inline
      // styles for now to achieve the desired appearance, but bug 1823489
      // will investigate having MigrationWizard own the <xul:panel>,
      // <panel-list> and <panel-item>'s so that styling can be done in the
      // stylesheet instead.
      let button = opt.shadowRoot.querySelector("button");
      button.style.minHeight = "40px";
      if (migrator.brandImage) {
        button.style.backgroundImage = `url(${migrator.brandImage})`;
      } else {
        button.style.backgroundImage = `url("chrome://global/skin/icons/defaultFavicon.svg")`;
      }

      if (migrator.profile) {
        document.l10n.setAttributes(
          opt,
          "migration-wizard-selection-option-with-profile",
          {
            sourceBrowser: migrator.displayName,
            profileName: migrator.profile.name,
          }
        );
      } else {
        document.l10n.setAttributes(
          opt,
          "migration-wizard-selection-option-without-profile",
          {
            sourceBrowser: migrator.displayName,
          }
        );
      }

      this.#browserProfileSelectorList.appendChild(opt);
    }

    if (state.migrators.length) {
      this.#onBrowserProfileSelectionChanged(
        this.#browserProfileSelectorList.firstElementChild
      );
    }
  }

  /**
   * @typedef {object} ProgressState
   *   The migration progress state for a resource.
   * @property {boolean} inProgress
   *   True if progress is still underway.
   * @property {string} [message=undefined]
   *   An optional message to display underneath the resource in
   *   the progress dialog. This message is only shown when inProgress
   *   is `false`.
   */

  /**
   * Called when showing the progress / success page of the wizard.
   *
   * @param {object} state
   *   The state object passed into setState. The following properties are
   *   used:
   * @param {Object<string, ProgressState>} state.progress
   *   An object whose keys match one of DISPLAYED_RESOURCE_TYPES.
   *
   *   Any resource type not included in state.progress will be hidden.
   */
  #onShowingProgress(state) {
    // Any resource progress group not included in state.progress is hidden.
    let resourceGroups = this.#shadowRoot.querySelectorAll(
      ".resource-progress-group"
    );
    let totalProgressGroups = Object.keys(state.progress).length;
    let remainingProgressGroups = totalProgressGroups;

    for (let group of resourceGroups) {
      let resourceType = group.dataset.resourceType;
      if (!state.progress.hasOwnProperty(resourceType)) {
        group.hidden = true;
        continue;
      }
      group.hidden = false;

      let progressIcon = group.querySelector(".progress-icon");
      let successText = group.querySelector(".success-text");

      if (state.progress[resourceType].inProgress) {
        document.l10n.setAttributes(
          progressIcon,
          "migration-wizard-progress-icon-in-progress"
        );
        progressIcon.classList.remove("completed");
        // With no status text, we re-insert the &nbsp; so that the status
        // text area does not fully collapse.
        successText.appendChild(document.createTextNode("\u00A0"));
      } else {
        document.l10n.setAttributes(
          progressIcon,
          "migration-wizard-progress-icon-completed"
        );
        progressIcon.classList.add("completed");
        successText.textContent = state.progress[resourceType].message;
        remainingProgressGroups--;
      }
    }

    let migrationDone = remainingProgressGroups == 0;
    let headerL10nID = migrationDone
      ? "migration-wizard-progress-done-header"
      : "migration-wizard-progress-header";
    let header = this.#shadowRoot.getElementById("progress-header");
    document.l10n.setAttributes(header, headerL10nID);

    let progressPage = this.#shadowRoot.querySelector(
      "div[name='page-progress']"
    );
    let doneButton = progressPage.querySelector("#done-button");
    let cancelButton = progressPage.querySelector(".cancel-close");
    doneButton.hidden = !migrationDone;
    cancelButton.hidden = migrationDone;
  }

  /**
   * Certain parts of the MigrationWizard need to be modified slightly
   * in order to work properly with Storybook. This method should be called
   * to apply those changes after changing state.
   */
  #updateForStorybook() {
    // The CSS mask used for the progress spinner cannot be loaded via
    // chrome:// URIs in Storybook. We work around this by exposing the
    // progress elements as custom parts that the MigrationWizard story
    // can style on its own.
    this.#shadowRoot.querySelectorAll(".progress-icon").forEach(progressEl => {
      if (progressEl.classList.contains("completed")) {
        progressEl.removeAttribute("part");
      } else {
        progressEl.setAttribute("part", "progress-spinner");
      }
    });
  }

  /**
   * Takes the current state of the selections page and bundles them
   * up into a MigrationWizard:BeginMigration event that can be handled
   * externally to perform the actual migration.
   */
  #doImport() {
    let migrationEventDetail = this.#gatherMigrationEventDetails();

    this.dispatchEvent(
      new CustomEvent("MigrationWizard:BeginMigration", {
        bubbles: true,
        detail: migrationEventDetail,
      })
    );
  }

  /**
   * @typedef {object} MigrationDetails
   * @property {string} key
   *   The key for a MigratorBase subclass.
   * @property {object|null} profile
   *   A representation of a browser profile. This is serialized and originally
   *   sent down from the parent via the GetAvailableMigrators message.
   * @property {string[]} resourceTypes
   *   An array of resource types that the user is attempted to import. These
   *   strings should be from MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
   * @property {boolean} hasPermissions
   *   True if this MigrationWizardChild told us that the associated
   *   MigratorBase subclass for the key has enough permission to read
   *   the requested resources.
   */

  /**
   * Pulls information from the DOM state of the MigrationWizard and constructs
   * and returns an object that can be used to begin migration via and event
   * sent to the MigrationWizardChild.
   *
   * @returns {MigrationDetails} details
   */
  #gatherMigrationEventDetails() {
    let panelItem = this.#browserProfileSelector.selectedPanelItem;
    let key = panelItem.getAttribute("key");
    let profile = panelItem.profile;
    let hasPermissions = panelItem.hasPermissions;

    let resourceTypeFields = this.#resourceTypeList.querySelectorAll(
      "label[data-resource-type]"
    );
    let resourceTypes = [];
    for (let resourceTypeField of resourceTypeFields) {
      if (resourceTypeField.control.checked) {
        resourceTypes.push(resourceTypeField.dataset.resourceType);
      }
    }

    return {
      key,
      profile,
      resourceTypes,
      hasPermissions,
    };
  }

  /**
   * Sends a request to gain read access to the Safari profile folder on
   * macOS, and upon gaining access, performs a migration using the current
   * settings as gathered by #gatherMigrationEventDetails
   */
  #requestSafariPermissions() {
    let migrationEventDetail = this.#gatherMigrationEventDetails();
    this.dispatchEvent(
      new CustomEvent("MigrationWizard:RequestSafariPermissions", {
        bubbles: true,
        detail: migrationEventDetail,
      })
    );
  }

  /**
   * Changes selected-data-header text and selected-data text based on
   * how many resources are checked
   */
  async #displaySelectedResources() {
    let resourceTypeLabels = this.#resourceTypeList.querySelectorAll(
      "label:not([hidden])[data-resource-type]"
    );

    let totalResources = resourceTypeLabels.length;
    let checkedResources = 0;

    let selectedData = this.#shadowRoot.querySelector(".selected-data");
    let selectedDataArray = [];
    const RESOURCE_TYPE_TO_LABEL_IDS = {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]:
        "migration-list-bookmark-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]:
        "migration-list-password-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]:
        "migration-list-history-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]:
        "migration-list-autofill-label",
    };

    let resourceTypes = Object.keys(RESOURCE_TYPE_TO_LABEL_IDS);
    let labelIds = Object.values(RESOURCE_TYPE_TO_LABEL_IDS).map(id => {
      return { id };
    });
    let labels = await document.l10n.formatValues(labelIds);
    let resourceTypeLabelMapping = new Map();
    for (let i = 0; i < resourceTypes.length; ++i) {
      let resourceType = resourceTypes[i];
      resourceTypeLabelMapping.set(resourceType, labels[i]);
    }
    let formatter = new Intl.ListFormat(undefined, {
      style: "long",
      type: "conjunction",
    });
    for (let resourceTypeLabel of resourceTypeLabels) {
      if (resourceTypeLabel.control.checked) {
        selectedDataArray.push(
          resourceTypeLabelMapping.get(resourceTypeLabel.dataset.resourceType)
        );
        checkedResources++;
      }
    }
    if (selectedDataArray.length) {
      selectedDataArray[0] =
        selectedDataArray[0].charAt(0).toLocaleUpperCase() +
        selectedDataArray[0].slice(1);
      selectedData.textContent = formatter.format(selectedDataArray);
    } else {
      selectedData.textContent = "\u00A0";
    }

    let selectedDataHeader = this.#shadowRoot.querySelector(
      ".selected-data-header"
    );

    let importButton = this.#shadowRoot.querySelector("#import");
    importButton.disabled = checkedResources == 0;

    if (checkedResources == 0) {
      document.l10n.setAttributes(
        selectedDataHeader,
        "migration-no-selected-data-label"
      );
    } else if (checkedResources < totalResources) {
      document.l10n.setAttributes(
        selectedDataHeader,
        "migration-selected-data-label"
      );
    } else {
      document.l10n.setAttributes(
        selectedDataHeader,
        "migration-all-available-data-label"
      );
    }

    let selectionPage = this.#shadowRoot.querySelector(
      "div[name='page-selection']"
    );
    selectionPage.toggleAttribute("single-item", totalResources == 1);

    this.dispatchEvent(
      new CustomEvent("MigrationWizard:ResourcesUpdated", { bubbles: true })
    );
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        if (event.target == this.#importButton) {
          this.#doImport();
        } else if (
          event.target.classList.contains("cancel-close") ||
          event.target.id == "done-button"
        ) {
          this.dispatchEvent(
            new CustomEvent("MigrationWizard:Close", { bubbles: true })
          );
        } else if (event.target == this.#browserProfileSelector) {
          this.#browserProfileSelectorList.show(event);
        } else if (
          event.currentTarget == this.#browserProfileSelectorList &&
          event.target != this.#browserProfileSelectorList
        ) {
          this.#onBrowserProfileSelectionChanged(event.target);
        } else if (event.target == this.#safariPermissionButton) {
          this.#requestSafariPermissions();
        }
        break;
      }
      case "change": {
        if (event.target == this.#browserProfileSelector) {
          this.#onBrowserProfileSelectionChanged();
        } else if (event.target == this.#selectAllCheckbox) {
          let checkboxes = this.#shadowRoot.querySelectorAll(
            'label[data-resource-type]:not([hidden]) > input[type="checkbox"]'
          );
          for (let checkbox of checkboxes) {
            checkbox.checked = this.#selectAllCheckbox.checked;
          }
          this.#displaySelectedResources();
        } else {
          let checkboxes = this.#shadowRoot.querySelectorAll(
            'label[data-resource-type]:not([hidden]) > input[type="checkbox"]'
          );

          let allVisibleChecked = Array.from(checkboxes).every(checkbox => {
            return checkbox.checked;
          });

          this.#selectAllCheckbox.checked = allVisibleChecked;
          this.#displaySelectedResources();
        }
        break;
      }
    }
  }
}

if (globalThis.customElements) {
  customElements.define("migration-wizard", MigrationWizard);
}
