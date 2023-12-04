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
  #importFromFileButton = null;
  #chooseImportFromFile = null;
  #getPermissionsButton = null;
  #safariPermissionButton = null;
  #safariPasswordImportSkipButton = null;
  #safariPasswordImportSelectButton = null;
  #selectAllCheckbox = null;
  #resourceSummary = null;
  #expandedDetails = false;
  #extensionsSuccessLink = null;

  static get markup() {
    return `
      <template>
        <link rel="stylesheet" href="chrome://browser/skin/migration/migration-wizard.css">
        <named-deck id="wizard-deck" selected-view="page-loading" aria-busy="true" part="deck">
          <div name="page-loading">
            <h1 data-l10n-id="migration-wizard-selection-header" part="header"></h1>
            <div class="loading-block large"></div>
            <div class="loading-block small"></div>
            <div class="loading-block small"></div>
            <moz-button-group class="buttons" part="buttons">
              <!-- If possible, use the same button labels as the SELECTION page with the same strings.
                   That'll prevent flicker when the load state exits if we then enter the SELECTION page. -->
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label" disabled></button>
              <button data-l10n-id="migration-import-button-label" disabled></button>
            </moz-button-group>
          </div>

          <div name="page-selection">
            <h1 data-l10n-id="migration-wizard-selection-header" part="header"></h1>
            <button id="browser-profile-selector" aria-haspopup="menu" aria-labelledby="migrator-name profile-name">
              <span class="migrator-icon" role="img"></span>
              <div class="migrator-description" role="presentation">
                <div id="migrator-name">&nbsp;</div>
                <div id="profile-name" class="deemphasized-text"></div>
              </div>
              <span class="dropdown-icon" role="img"></span>
            </button>
            <div class="no-resources-found error-message">
              <span class="error-icon" role="img"></span>
              <div data-l10n-id="migration-wizard-import-browser-no-resources"></div>
            </div>

            <div class="no-permissions-message">
              <p data-l10n-id="migration-no-permissions-message">
              </p>
              <p data-l10n-id="migration-no-permissions-instructions">
              </p>
              <ol>
                <li data-l10n-id="migration-no-permissions-instructions-step1"></li>
                <li class="migration-no-permissions-instructions-step2" data-l10n-id="migration-no-permissions-instructions-step2" data-l10n-args='{"permissionsPath": "" }'>
                  <code></code>
                </li>
              </ol>
            </div>

            <div data-l10n-id="migration-wizard-selection-list" class="resource-selection-preamble deemphasized-text hide-on-error"></div>
            <details class="resource-selection-details hide-on-error">
              <summary id="resource-selection-summary">
                <div class="selected-data-header" data-l10n-id="migration-all-available-data-label"></div>
                <div class="selected-data deemphasized-text">&nbsp;</div>
                <span class="expand-collapse-icon" role="img"></span>
              </summary>
              <fieldset id="resource-type-list">
                <label id="select-all">
                  <input type="checkbox" class="select-all-checkbox"/><span data-l10n-id="migration-select-all-option-label"></span>
                </label>
                <label id="bookmarks" data-resource-type="BOOKMARKS"/>
                  <input type="checkbox"/><span default-data-l10n-id="migration-bookmarks-option-label" ie-edge-data-l10n-id="migration-favorites-option-label"></span>
                </label>
                <label id="logins-and-passwords" data-resource-type="PASSWORDS">
                  <input type="checkbox"/><span data-l10n-id="migration-passwords-option-label"></span>
                </label>
                <label id="history" data-resource-type="HISTORY">
                  <input type="checkbox"/><span data-l10n-id="migration-history-option-label"></span>
                </label>
                <label id="extensions" data-resource-type="EXTENSIONS">
                  <input type="checkbox"/><span data-l10n-id="migration-extensions-option-label"></span>
                </label>
                <label id="form-autofill" data-resource-type="FORMDATA">
                  <input type="checkbox"/><span data-l10n-id="migration-form-autofill-option-label"></span>
                </label>
                <label id="payment-methods" data-resource-type="PAYMENT_METHODS">
                  <input type="checkbox"/><span data-l10n-id="migration-payment-methods-option-label"></span>
                </label>
              </fieldset>
            </details>

            <div class="file-import-error error-message">
              <span class="error-icon" role="img"></span>
              <div id="file-import-error-message"></div>
            </div>

            <moz-button-group class="buttons" part="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label"></button>
              <button id="import-from-file" class="primary" data-l10n-id="migration-import-from-file-button-label"></button>
              <button id="import" class="primary" data-l10n-id="migration-import-button-label"></button>
              <button id="get-permissions" class="primary" data-l10n-id="migration-continue-button-label"></button>
            </moz-button-group>
          </div>

          <div name="page-progress">
            <h1 id="progress-header" data-l10n-id="migration-wizard-progress-header" part="header"></h1>
            <div class="resource-progress">
              <div data-resource-type="BOOKMARKS" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span default-data-l10n-id="migration-bookmarks-option-label" ie-edge-data-l10n-id="migration-favorites-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="PASSWORDS" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-passwords-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="HISTORY" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-history-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="EXTENSIONS" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-extensions-option-label"></span>
                <a id="extensions-success-link" href="about:addons" class="message-text deemphasized-text"></a>
                <span class="message-text deemphasized-text"></span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="FORMDATA" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-form-autofill-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="PAYMENT_METHODS" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-payment-methods-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="COOKIES" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-cookies-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="SESSION" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-session-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>

              <div data-resource-type="OTHERDATA" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-otherdata-option-label"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
                <a class="support-text deemphasized-text"></a>
              </div>
            </div>
            <moz-button-group class="buttons" part="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label" disabled></button>
              <button class="primary finish-button done-button" data-l10n-id="migration-done-button-label"></button>
              <button class="primary finish-button continue-button" data-l10n-id="migration-continue-button-label"></button>
            </moz-button-group>
          </div>

          <div name="page-file-import-progress">
            <h1 id="file-import-progress-header"part="header"></h1>
            <div class="resource-progress">
              <div data-resource-type="PASSWORDS_FROM_FILE" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-passwords-from-file"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
              </div>

              <div data-resource-type="PASSWORDS_NEW" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-passwords-new"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
              </div>

              <div data-resource-type="PASSWORDS_UPDATED" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-passwords-updated"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
              </div>

              <div data-resource-type="BOOKMARKS_FROM_FILE" class="resource-progress-group">
                <span class="progress-icon-parent"><span class="progress-icon" role="img"></span></span>
                <span data-l10n-id="migration-bookmarks-from-file"></span>
                <span class="message-text deemphasized-text">&nbsp;</span>
              </div>
            </div>
            <moz-button-group class="buttons" part="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label" disabled></button>
              <button class="primary finish-button done-button" data-l10n-id="migration-done-button-label"></button>
              <button class="primary finish-button continue-button" data-l10n-id="migration-continue-button-label"></button>
            </moz-button-group>
          </div>

          <div name="page-safari-password-permission">
            <h1 data-l10n-id="migration-safari-password-import-header" part="header"></h1>
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
            <moz-button-group class="buttons" part="buttons">
              <button id="safari-password-import-skip" data-l10n-id="migration-safari-password-import-skip-button"></button>
              <button id="safari-password-import-select" class="primary" data-l10n-id="migration-safari-password-import-select-button"></button>
            </moz-button-group>
          </div>

          <div name="page-safari-permission">
            <h1 data-l10n-id="migration-wizard-selection-header" part="header"></h1>
            <div data-l10n-id="migration-wizard-safari-permissions-sub-header"></div>
            <ol>
              <li data-l10n-id="migration-wizard-safari-instructions-continue"></li>
              <li data-l10n-id="migration-wizard-safari-instructions-folder"></li>
            </ol>
            <moz-button-group class="buttons" part="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label"></button>
              <button id="safari-request-permissions" class="primary" data-l10n-id="migration-continue-button-label"></button>
            </moz-button-group>
          </div>

          <div name="page-no-browsers-found">
            <h1 data-l10n-id="migration-wizard-selection-header" part="header"></h1>
            <div class="no-browsers-found error-message">
              <span class="error-icon" role="img"></span>
              <div class="no-browsers-found-message" data-l10n-id="migration-wizard-import-browser-no-browsers"></div>
            </div>
            <moz-button-group class="buttons" part="buttons">
              <button class="cancel-close" data-l10n-id="migration-cancel-button-label"></button>
              <button id="choose-import-from-file" class="primary" data-l10n-id="migration-choose-to-import-from-file-button-label"></button>
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
    return MigrationWizard.#template.content.cloneNode(true);
  }

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open" });

    if (window.MozXULElement) {
      window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");
      window.MozXULElement.insertFTLIfNeeded("browser/migrationWizard.ftl");
    }
    document.l10n.connectRoot(shadow);

    shadow.appendChild(MigrationWizard.fragment);

    this.#deck = shadow.querySelector("#wizard-deck");
    this.#browserProfileSelector = shadow.querySelector(
      "#browser-profile-selector"
    );
    this.#resourceSummary = shadow.querySelector("#resource-selection-summary");
    this.#resourceSummary.addEventListener("click", this);

    let cancelCloseButtons = shadow.querySelectorAll(".cancel-close");
    for (let button of cancelCloseButtons) {
      button.addEventListener("click", this);
    }

    let finishButtons = shadow.querySelectorAll(".finish-button");
    for (let button of finishButtons) {
      button.addEventListener("click", this);
    }

    this.#importButton = shadow.querySelector("#import");
    this.#importButton.addEventListener("click", this);
    this.#importFromFileButton = shadow.querySelector("#import-from-file");
    this.#importFromFileButton.addEventListener("click", this);
    this.#chooseImportFromFile = shadow.querySelector(
      "#choose-import-from-file"
    );
    this.#chooseImportFromFile.addEventListener("click", this);
    this.#getPermissionsButton = shadow.querySelector("#get-permissions");
    this.#getPermissionsButton.addEventListener("click", this);

    this.#browserProfileSelector.addEventListener("click", this);
    this.#resourceTypeList = shadow.querySelector("#resource-type-list");
    this.#resourceTypeList.addEventListener("change", this);

    this.#safariPermissionButton = shadow.querySelector(
      "#safari-request-permissions"
    );
    this.#safariPermissionButton.addEventListener("click", this);

    this.#selectAllCheckbox = shadow.querySelector("#select-all").control;

    this.#safariPasswordImportSkipButton = shadow.querySelector(
      "#safari-password-import-skip"
    );
    this.#safariPasswordImportSkipButton.addEventListener("click", this);

    this.#safariPasswordImportSelectButton = shadow.querySelector(
      "#safari-password-import-select"
    );
    this.#safariPasswordImportSelectButton.addEventListener("click", this);

    this.#extensionsSuccessLink = shadow.querySelector(
      "#extensions-success-link"
    );
    this.#extensionsSuccessLink.addEventListener("click", this);

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
      case MigrationWizardConstants.PAGES.FILE_IMPORT_PROGRESS: {
        this.#onShowingFileImportProgress(state);
        break;
      }
      case MigrationWizardConstants.PAGES.NO_BROWSERS_FOUND: {
        this.#onShowingNoBrowsersFound(state);
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

  get #dialogMode() {
    return this.hasAttribute("dialog-mode");
  }

  #ensureSelectionDropdown() {
    if (this.#browserProfileSelectorList) {
      return;
    }
    this.#browserProfileSelectorList = document.createElement("panel-list");
    this.#browserProfileSelectorList.toggleAttribute(
      "min-width-from-anchor",
      true
    );
    this.#browserProfileSelectorList.addEventListener("click", this);

    if (document.createXULElement) {
      let panel = document.createXULElement("panel");
      panel.appendChild(this.#browserProfileSelectorList);
      this.#shadowRoot.appendChild(panel);
    } else {
      this.#shadowRoot.appendChild(this.#browserProfileSelectorList);
    }
  }

  /**
   * Reacts to changes to the browser / profile selector dropdown. This
   * should update the list of resource types to match what's supported
   * by the selected migrator and profile.
   *
   *  @param {Element} panelItem the selected <panel-item>
   */
  #onBrowserProfileSelectionChanged(panelItem) {
    this.#browserProfileSelector.selectedPanelItem = panelItem;
    this.#browserProfileSelector.querySelector("#migrator-name").textContent =
      panelItem.displayName;
    this.#browserProfileSelector.querySelector("#profile-name").textContent =
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

    let key = panelItem.getAttribute("key");
    let resourceTypes = panelItem.resourceTypes;

    for (let child of this.#resourceTypeList.querySelectorAll(
      "label[data-resource-type]"
    )) {
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

        let labelSpan = resourceLabel.querySelector(
          "span[default-data-l10n-id]"
        );
        if (labelSpan) {
          if (MigrationWizardConstants.USES_FAVORITES.includes(key)) {
            document.l10n.setAttributes(
              labelSpan,
              labelSpan.getAttribute("ie-edge-data-l10n-id")
            );
          } else {
            document.l10n.setAttributes(
              labelSpan,
              labelSpan.getAttribute("default-data-l10n-id")
            );
          }
        }
      }
    }
    let selectAll = this.#shadowRoot.querySelector("#select-all").control;
    selectAll.checked = true;

    this.#displaySelectedResources();
    this.#browserProfileSelector.selectedPanelItem = panelItem;

    let selectionPage = this.#shadowRoot.querySelector(
      "div[name='page-selection']"
    );
    selectionPage.setAttribute("migrator-type", panelItem.getAttribute("type"));

    // Safari currently has a special flow for requesting permissions that
    // occurs _after_ resource selection, so we don't show this message
    // for that migrator.
    let showNoPermissionsMessage =
      panelItem.getAttribute("type") ==
        MigrationWizardConstants.MIGRATOR_TYPES.BROWSER &&
      !panelItem.hasPermissions &&
      panelItem.getAttribute("key") != "safari";

    selectionPage.toggleAttribute("no-permissions", showNoPermissionsMessage);
    if (showNoPermissionsMessage) {
      let step2 = selectionPage.querySelector(
        ".migration-no-permissions-instructions-step2"
      );
      step2.setAttribute(
        "data-l10n-args",
        JSON.stringify({ permissionsPath: panelItem.permissionsPath })
      );

      this.dispatchEvent(
        new CustomEvent("MigrationWizard:PermissionsNeeded", {
          bubbles: true,
          detail: {
            key,
          },
        })
      );
    }

    selectionPage.toggleAttribute(
      "no-resources",
      panelItem.getAttribute("type") ==
        MigrationWizardConstants.MIGRATOR_TYPES.BROWSER &&
        !resourceTypes.length &&
        panelItem.hasPermissions
    );
  }

  /**
   * Called when showing the browser/profile selection page of the wizard.
   *
   * @param {object} state
   *   The state object passed into setState. The following properties are
   *   used:
   * @param {string[]} state.migrators
   *   An array of source browser names that can be migrated from.
   * @param {string} [state.migratorKey=null]
   *   The key for a migrator to automatically select in the migrators array.
   *   If not defined, the first item in the array will be selected.
   * @param {string} [state.fileImportErrorMessage=null]
   *   An error message to display in the event that an attempt at doing a
   *   file import failed. File import failures are special in that they send
   *   the wizard back to the selection page with an error message. If not
   *   defined, it is presumed that a file import error has not occurred.
   */
  #onShowingSelection(state) {
    this.#ensureSelectionDropdown();
    this.#browserProfileSelectorList.textContent = "";

    let selectionPage = this.#shadowRoot.querySelector(
      "div[name='page-selection']"
    );

    let details = this.#shadowRoot.querySelector("details");

    if (this.hasAttribute("force-show-import-all")) {
      let forceShowImportAll =
        this.getAttribute("force-show-import-all") == "true";
      selectionPage.toggleAttribute("show-import-all", forceShowImportAll);
      details.open = !forceShowImportAll;
    } else {
      selectionPage.toggleAttribute("show-import-all", state.showImportAll);
      details.open = !state.showImportAll;
    }

    this.#expandedDetails = false;

    for (let migrator of state.migrators) {
      let opt = document.createElement("panel-item");
      opt.setAttribute("key", migrator.key);
      opt.setAttribute("type", migrator.type);
      opt.profile = migrator.profile;
      opt.displayName = migrator.displayName;
      opt.resourceTypes = migrator.resourceTypes;
      opt.hasPermissions = migrator.hasPermissions;
      opt.permissionsPath = migrator.permissionsPath;
      opt.brandImage = migrator.brandImage;

      let button = opt.shadowRoot.querySelector("button");
      if (migrator.brandImage) {
        button.style.backgroundImage = `url(${migrator.brandImage})`;
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

    if (state.migratorKey) {
      let panelItem = this.#browserProfileSelectorList.querySelector(
        `panel-item[key="${state.migratorKey}"]`
      );
      this.#onBrowserProfileSelectionChanged(panelItem);
    }

    let fileImportErrorMessageEl = selectionPage.querySelector(
      "#file-import-error-message"
    );

    if (state.fileImportErrorMessage) {
      fileImportErrorMessageEl.textContent = state.fileImportErrorMessage;
      selectionPage.toggleAttribute("file-import-error", true);
    } else {
      fileImportErrorMessageEl.textContent = "";
      selectionPage.toggleAttribute("file-import-error", false);
    }

    // Since this is called before the named-deck actually switches to
    // show the selection page, we cannot focus this button immediately.
    // Instead, we use a rAF to queue this up for focusing before the
    // next paint.
    requestAnimationFrame(() => {
      this.#browserProfileSelector.focus({ focusVisible: false });
    });
  }

  /**
   * @typedef {object} ProgressState
   *  The migration progress state for a resource.
   * @property {number} value
   *  One of the values from MigrationWizardConstants.PROGRESS_VALUE.
   * @property {string} [message=undefined]
   *  An optional message to display underneath the resource in
   *  the progress dialog. This message is only shown when value
   *  is not LOADING.
   * @property {string} [linkURL=undefined]
   *  The URL for an optional link to appear after the status message.
   *  This will only be shown if linkText is also not-empty.
   * @property {string} [linkText=undefined]
   *  The text for an optional link to appear after the status message.
   *  This will only be shown if linkURL is also not-empty.
   */

  /**
   * Called when showing the progress / success page of the wizard.
   *
   * @param {object} state
   *   The state object passed into setState. The following properties are
   *   used:
   * @param {string} state.key
   *   The key of the migrator being used.
   * @param {Object<string, ProgressState>} state.progress
   *   An object whose keys match one of DISPLAYED_RESOURCE_TYPES.
   *
   *   Any resource type not included in state.progress will be hidden.
   */
  #onShowingProgress(state) {
    // Any resource progress group not included in state.progress is hidden.
    let progressPage = this.#shadowRoot.querySelector(
      "div[name='page-progress']"
    );
    let resourceGroups = progressPage.querySelectorAll(
      ".resource-progress-group"
    );
    this.#extensionsSuccessLink.textContent = "";

    let totalProgressGroups = Object.keys(state.progress).length;
    let remainingProgressGroups = totalProgressGroups;
    let totalWarnings = 0;

    for (let group of resourceGroups) {
      let resourceType = group.dataset.resourceType;
      if (!state.progress.hasOwnProperty(resourceType)) {
        group.hidden = true;
        continue;
      }
      group.hidden = false;

      let progressIcon = group.querySelector(".progress-icon");
      let messageText = group.querySelector("span.message-text");
      let supportLink = group.querySelector(".support-text");

      let labelSpan = group.querySelector("span[default-data-l10n-id]");
      if (labelSpan) {
        if (MigrationWizardConstants.USES_FAVORITES.includes(state.key)) {
          document.l10n.setAttributes(
            labelSpan,
            labelSpan.getAttribute("ie-edge-data-l10n-id")
          );
        } else {
          document.l10n.setAttributes(
            labelSpan,
            labelSpan.getAttribute("default-data-l10n-id")
          );
        }
      }
      messageText.textContent = "";

      if (supportLink) {
        supportLink.textContent = "";
        supportLink.removeAttribute("href");
      }
      let progressValue = state.progress[resourceType].value;
      switch (progressValue) {
        case MigrationWizardConstants.PROGRESS_VALUE.LOADING: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-in-progress"
          );
          progressIcon.setAttribute("state", "loading");
          messageText.textContent = "";
          supportLink.textContent = "";
          supportLink.removeAttribute("href");
          // With no status text, we re-insert the &nbsp; so that the status
          // text area does not fully collapse.
          messageText.appendChild(document.createTextNode("\u00A0"));
          break;
        }
        case MigrationWizardConstants.PROGRESS_VALUE.SUCCESS: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-completed"
          );
          progressIcon.setAttribute("state", "success");
          messageText.textContent = state.progress[resourceType].message;
          if (
            resourceType ==
            MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS
          ) {
            messageText.textContent = "";
            this.#extensionsSuccessLink.textContent =
              state.progress[resourceType].message;
          }
          remainingProgressGroups--;
          break;
        }
        case MigrationWizardConstants.PROGRESS_VALUE.WARNING: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-completed"
          );
          progressIcon.setAttribute("state", "warning");
          messageText.textContent = state.progress[resourceType].message;
          supportLink.textContent = state.progress[resourceType].linkText;
          supportLink.href = state.progress[resourceType].linkURL;
          remainingProgressGroups--;
          totalWarnings++;
          break;
        }
        case MigrationWizardConstants.PROGRESS_VALUE.INFO: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-completed"
          );
          progressIcon.setAttribute("state", "info");
          messageText.textContent = state.progress[resourceType].message;
          supportLink.textContent = state.progress[resourceType].linkText;
          supportLink.href = state.progress[resourceType].linkURL;
          if (
            resourceType ==
            MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS
          ) {
            messageText.textContent = "";
            this.#extensionsSuccessLink.textContent =
              state.progress[resourceType].message;
          }
          remainingProgressGroups--;
          break;
        }
      }
    }

    let migrationDone = remainingProgressGroups == 0;
    let headerL10nID = "migration-wizard-progress-header";

    if (migrationDone) {
      if (totalWarnings) {
        headerL10nID = "migration-wizard-progress-done-with-warnings-header";
      } else {
        headerL10nID = "migration-wizard-progress-done-header";
      }
    }

    let header = this.#shadowRoot.getElementById("progress-header");
    document.l10n.setAttributes(header, headerL10nID);

    let finishButtons = progressPage.querySelectorAll(".finish-button");
    let cancelButton = progressPage.querySelector(".cancel-close");

    for (let finishButton of finishButtons) {
      finishButton.hidden = !migrationDone;
    }

    cancelButton.hidden = migrationDone;

    if (migrationDone) {
      // Since this might be called before the named-deck actually switches to
      // show the progress page, we cannot focus this button immediately.
      // Instead, we use a rAF to queue this up for focusing before the
      // next paint.
      requestAnimationFrame(() => {
        let button = this.#dialogMode
          ? progressPage.querySelector(".done-button")
          : progressPage.querySelector(".continue-button");
        button.focus({ focusVisible: false });
      });
    }
  }

  /**
   * Called when showing the progress / success page of the wizard for
   * files.
   *
   * @param {object} state
   *   The state object passed into setState. The following properties are
   *   used:
   * @param {string} state.title
   *   The string to display in the header.
   * @param {Object<string, ProgressState>} state.progress
   *   An object whose keys match one of DISPLAYED_FILE_RESOURCE_TYPES.
   *
   *   Any resource type not included in state.progress will be hidden.
   */
  #onShowingFileImportProgress(state) {
    // Any resource progress group not included in state.progress is hidden.
    let progressPage = this.#shadowRoot.querySelector(
      "div[name='page-file-import-progress']"
    );
    let resourceGroups = progressPage.querySelectorAll(
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
      let messageText = group.querySelector(".message-text");

      let progressValue = state.progress[resourceType].value;
      switch (progressValue) {
        case MigrationWizardConstants.PROGRESS_VALUE.LOADING: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-in-progress"
          );
          progressIcon.setAttribute("state", "loading");
          messageText.textContent = "";
          // With no status text, we re-insert the &nbsp; so that the status
          // text area does not fully collapse.
          messageText.appendChild(document.createTextNode("\u00A0"));
          break;
        }
        case MigrationWizardConstants.PROGRESS_VALUE.SUCCESS: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-completed"
          );
          progressIcon.setAttribute("state", "success");
          messageText.textContent = state.progress[resourceType].message;
          remainingProgressGroups--;
          break;
        }
        case MigrationWizardConstants.PROGRESS_VALUE.WARNING: {
          document.l10n.setAttributes(
            progressIcon,
            "migration-wizard-progress-icon-completed"
          );
          progressIcon.setAttribute("state", "warning");
          messageText.textContent = state.progress[resourceType].message;
          remainingProgressGroups--;
          break;
        }
        default: {
          console.error(
            "Unrecognized state for file migration: ",
            progressValue
          );
        }
      }
    }

    let migrationDone = remainingProgressGroups == 0;
    let header = this.#shadowRoot.getElementById("file-import-progress-header");
    header.textContent = state.title;

    let doneButton = progressPage.querySelector(".primary");
    let cancelButton = progressPage.querySelector(".cancel-close");
    doneButton.hidden = !migrationDone;
    cancelButton.hidden = migrationDone;

    if (migrationDone) {
      // Since this might be called before the named-deck actually switches to
      // show the progress page, we cannot focus this button immediately.
      // Instead, we use a rAF to queue this up for focusing before the
      // next paint.
      requestAnimationFrame(() => {
        doneButton.focus({ focusVisible: false });
      });
    }
  }

  /**
   * Called when showing the "no browsers found" page of the wizard.
   *
   * @param {object} state
   *   The state object passed into setState. The following properties are
   *   used:
   * @param {string} state.hasFileMigrators
   *   True if at least one FileMigrator is available for use.
   */
  #onShowingNoBrowsersFound(state) {
    this.#chooseImportFromFile.hidden = !state.hasFileMigrators;
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
      if (progressEl.getAttribute("state") == "loading") {
        progressEl.setAttribute("part", "progress-spinner");
      } else {
        progressEl.removeAttribute("part");
      }
    });
  }

  /**
   * A public method for starting a migration without the user needing
   * to choose a browser, profile or resource types. This is typically
   * done only for doing a profile reset.
   *
   * @param {string} migratorKey
   *   The key associated with the migrator to use.
   * @param {object|null} profile
   *   A representation of a browser profile. When not null, this is an
   *   object with a string "id" property, and a string "name" property.
   * @param {string[]} resourceTypes
   *   An array of resource types that import should occur for. These
   *   strings should be from MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
   */
  doAutoImport(migratorKey, profile, resourceTypes) {
    let migrationEventDetail = this.#gatherMigrationEventDetails({
      migratorKey,
      profile,
      resourceTypes,
    });

    this.dispatchEvent(
      new CustomEvent("MigrationWizard:BeginMigration", {
        bubbles: true,
        detail: migrationEventDetail,
      })
    );
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
   * @property {boolean} expandedDetails
   *   True if the user clicked on the <summary> element to expand the resource
   *   type list.
   * @property {boolean} autoMigration
   *   True if the migration is occurring automatically, without the user
   *   having selected any items explicitly from the wizard.
   * @property {string} [safariPasswordFilePath=null]
   *   An optional string argument that points to the path of a passwords
   *   export file from Safari. This file will have password imported from if
   *   supplied. This argument is ignored if the key is not for the
   *   Safari browser.
   */

  /**
   * Pulls information from the DOM state of the MigrationWizard and constructs
   * and returns an object that can be used to begin migration via and event
   * sent to the MigrationWizardChild. If autoMigrationDetails is provided,
   * this information is used to construct the object instead of the DOM state.
   *
   * @param {object} [autoMigrationDetails=null]
   *   Provided iff an automatic migration is being invoked. In that case, the
   *   details are constructed from this object rather than the wizard DOM state.
   * @param {string} autoMigrationDetails.migratorKey
   *   The key of the migrator to do automatic migration from.
   * @param {object|null} autoMigrationDetails.profile
   *   A representation of a browser profile. When not null, this is an
   *   object with a string "id" property, and a string "name" property.
   * @param {string[]} autoMigrationDetails.resourceTypes
   *   An array of resource types that import should occur for. These
   *   strings should be from MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
   * @returns {MigrationDetails} details
   */
  #gatherMigrationEventDetails(autoMigrationDetails) {
    if (autoMigrationDetails?.migratorKey) {
      let { migratorKey, profile, resourceTypes } = autoMigrationDetails;

      return {
        key: migratorKey,
        type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
        profile,
        resourceTypes,
        hasPermissions: true,
        expandedDetails: this.#expandedDetails,
        autoMigration: true,
      };
    }

    let panelItem = this.#browserProfileSelector.selectedPanelItem;
    let key = panelItem.getAttribute("key");
    let type = panelItem.getAttribute("type");
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
      type,
      profile,
      resourceTypes,
      hasPermissions,
      expandedDetails: this.#expandedDetails,
      autoMigration: false,
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
   * Sends a request to get a string path for a passwords file exported
   * from Safari.
   */
  #selectSafariPasswordFile() {
    let migrationEventDetail = this.#gatherMigrationEventDetails();
    this.dispatchEvent(
      new CustomEvent("MigrationWizard:SelectSafariPasswordFile", {
        bubbles: true,
        detail: migrationEventDetail,
      })
    );
  }

  /**
   * Sends a request to get read permissions for the data associated
   * with the selected browser.
   */
  #getPermissions() {
    let migrationEventDetail = this.#gatherMigrationEventDetails();
    this.dispatchEvent(
      new CustomEvent("MigrationWizard:GetPermissions", {
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
    let panelItem = this.#browserProfileSelector.selectedPanelItem;
    let key = panelItem.getAttribute("key");

    let totalResources = resourceTypeLabels.length;
    let checkedResources = 0;

    let selectedData = this.#shadowRoot.querySelector(".selected-data");
    let selectedDataArray = [];
    let resourceTypeToLabelIDs = {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]:
        "migration-list-bookmark-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]:
        "migration-list-password-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]:
        "migration-list-history-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]:
        "migration-list-extensions-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]:
        "migration-list-autofill-label",
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]:
        "migration-list-payment-methods-label",
    };

    if (MigrationWizardConstants.USES_FAVORITES.includes(key)) {
      resourceTypeToLabelIDs[
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS
      ] = "migration-list-favorites-label";
    }

    let resourceTypes = Object.keys(resourceTypeToLabelIDs);
    let labelIds = Object.values(resourceTypeToLabelIDs).map(id => {
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
        if (
          event.target == this.#importButton ||
          event.target == this.#importFromFileButton
        ) {
          this.#doImport();
        } else if (
          event.target.classList.contains("cancel-close") ||
          event.target.classList.contains("finish-button")
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
          // If the user selected a file migration type from the selector, we'll
          // help the user out by immediately starting the file migration flow,
          // rather than waiting for them to click the "Select File".
          if (
            event.target.getAttribute("type") ==
            MigrationWizardConstants.MIGRATOR_TYPES.FILE
          ) {
            this.#doImport();
          }
        } else if (event.target == this.#safariPermissionButton) {
          this.#requestSafariPermissions();
        } else if (event.currentTarget == this.#resourceSummary) {
          this.#expandedDetails = true;
        } else if (event.target == this.#chooseImportFromFile) {
          this.dispatchEvent(
            new CustomEvent("MigrationWizard:RequestState", {
              bubbles: true,
              detail: {
                allowOnlyFileMigrators: true,
              },
            })
          );
        } else if (event.target == this.#safariPasswordImportSkipButton) {
          // If the user chose to skip importing passwords from Safari, we
          // programmatically uncheck the PASSWORDS resource type and re-request
          // import.
          let checkbox = this.#shadowRoot.querySelector(
            `label[data-resource-type="${MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS}"]`
          ).control;
          checkbox.checked = false;

          // If there are no other checked checkboxes, go back to the selection
          // screen.
          let checked = this.#shadowRoot.querySelectorAll(
            `label[data-resource-type] > input:checked`
          ).length;

          if (!checked) {
            this.requestState();
          } else {
            this.#doImport();
          }
        } else if (event.target == this.#safariPasswordImportSelectButton) {
          this.#selectSafariPasswordFile();
        } else if (event.target == this.#extensionsSuccessLink) {
          this.dispatchEvent(
            new CustomEvent("MigrationWizard:OpenAboutAddons", {
              bubbles: true,
            })
          );
          event.preventDefault();
        } else if (event.target == this.#getPermissionsButton) {
          this.#getPermissions();
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
