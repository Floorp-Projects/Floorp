/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  recordTelemetryEvent,
  promptForMasterPassword,
} from "../aboutLoginsUtils.js";

export default class LoginItem extends HTMLElement {
  /**
   * The number of milliseconds to display the "Copied" success message
   * before reverting to the normal "Copy" button.
   */
  static get COPY_BUTTON_RESET_TIMEOUT() {
    return 5000;
  }

  constructor() {
    super();
    this._login = {};
    this._error = null;
    this._copyUsernameTimeoutId = 0;
    this._copyPasswordTimeoutId = 0;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      this.render();
      return;
    }

    let loginItemTemplate = document.querySelector("#login-item-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(loginItemTemplate.content.cloneNode(true));

    this._cancelButton = this.shadowRoot.querySelector(".cancel-button");
    this._confirmDeleteDialog = document.querySelector("confirm-delete-dialog");
    this._copyPasswordButton = this.shadowRoot.querySelector(
      ".copy-password-button"
    );
    this._copyUsernameButton = this.shadowRoot.querySelector(
      ".copy-username-button"
    );
    this._deleteButton = this.shadowRoot.querySelector(".delete-button");
    this._editButton = this.shadowRoot.querySelector(".edit-button");
    this._errorMessage = this.shadowRoot.querySelector(".error-message");
    this._errorMessageLink = this._errorMessage.querySelector(
      ".error-message-link"
    );
    this._errorMessageText = this._errorMessage.querySelector(
      ".error-message-text"
    );
    this._form = this.shadowRoot.querySelector("form");
    this._originInput = this.shadowRoot.querySelector("input[name='origin']");
    this._originDisplayInput = this.shadowRoot.querySelector(
      "a[name='origin']"
    );
    this._usernameInput = this.shadowRoot.querySelector(
      "input[name='username']"
    );
    // type=password field for display which only ever contains spaces the correct
    // length of the password.
    this._passwordDisplayInput = this.shadowRoot.querySelector(
      "input.password-display"
    );
    // type=text field for editing the password with the actual password value.
    this._passwordInput = this.shadowRoot.querySelector(
      "input[name='password']"
    );
    this._revealCheckbox = this.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    this._saveChangesButton = this.shadowRoot.querySelector(
      ".save-changes-button"
    );
    this._favicon = this.shadowRoot.querySelector(".login-item-favicon");
    this._faviconWrapper = this.shadowRoot.querySelector(
      ".login-item-favicon-wrapper"
    );
    this._title = this.shadowRoot.querySelector(".login-item-title");
    this._timeCreated = this.shadowRoot.querySelector(".time-created");
    this._timeChanged = this.shadowRoot.querySelector(".time-changed");
    this._timeUsed = this.shadowRoot.querySelector(".time-used");
    this._breachAlert = this.shadowRoot.querySelector(".breach-alert");
    this._breachAlertLink = this._breachAlert.querySelector(".alert-link");
    this._breachAlertDate = this._breachAlert.querySelector(".alert-date");
    this._breachAlertLearnMoreLink = this._breachAlert.querySelector(
      ".alert-learn-more-link"
    );
    this._vulnerableAlert = this.shadowRoot.querySelector(".vulnerable-alert");
    this._vulnerableAlertLink = this._vulnerableAlert.querySelector(
      ".alert-link"
    );
    this._vulnerableAlertLearnMoreLink = this._vulnerableAlert.querySelector(
      ".alert-learn-more-link"
    );

    this.render();

    this._breachAlertLearnMoreLink.addEventListener("click", this);
    this._cancelButton.addEventListener("click", this);
    this._copyPasswordButton.addEventListener("click", this);
    this._copyUsernameButton.addEventListener("click", this);
    this._deleteButton.addEventListener("click", this);
    this._editButton.addEventListener("click", this);
    this._errorMessageLink.addEventListener("click", this);
    this._form.addEventListener("submit", this);
    this._originInput.addEventListener("blur", this);
    this._originInput.addEventListener("click", this);
    this._originInput.addEventListener("mousedown", this, true);
    this._originInput.addEventListener("auxclick", this);
    this._originDisplayInput.addEventListener("click", this);
    this._revealCheckbox.addEventListener("click", this);
    this._vulnerableAlertLearnMoreLink.addEventListener("click", this);
    window.addEventListener("AboutLoginsInitialLoginSelected", this);
    window.addEventListener("AboutLoginsLoadInitialFavicon", this);
    window.addEventListener("AboutLoginsLoginSelected", this);
    window.addEventListener("AboutLoginsShowBlankLogin", this);
    window.addEventListener("AboutLoginsRemaskPassword", this);
  }

  focus() {
    if (!this._editButton.disabled) {
      this._editButton.focus();
    } else if (!this._deleteButton.disabled) {
      this._deleteButton.focus();
    } else {
      this._originInput.focus();
    }
  }

  async render(
    { onlyUpdateErrorsAndAlerts } = { onlyUpdateErrorsAndAlerts: false }
  ) {
    if (this._error) {
      if (this._error.errorMessage.includes("This login already exists")) {
        document.l10n.setAttributes(
          this._errorMessageLink,
          "about-logins-error-message-duplicate-login-with-link",
          {
            loginTitle: this._error.login.title,
          }
        );
        this._errorMessageLink.dataset.errorGuid = this._error.existingLoginGuid;
        this._errorMessageText.hidden = true;
        this._errorMessageLink.hidden = false;
      } else {
        this._errorMessageText.hidden = false;
        this._errorMessageLink.hidden = true;
      }
    }
    this._errorMessage.hidden = !this._error;

    this._breachAlert.hidden =
      !this._breachesMap || !this._breachesMap.has(this._login.guid);
    if (!this._breachAlert.hidden) {
      const breachDetails = this._breachesMap.get(this._login.guid);
      this._breachAlertLearnMoreLink.href = breachDetails.breachAlertURL;
      this._breachAlertLink.href = this._login.origin;
      document.l10n.setAttributes(
        this._breachAlertLink,
        "about-logins-breach-alert-link",
        { hostname: this._login.displayOrigin }
      );
      if (breachDetails.BreachDate) {
        let breachDate = new Date(breachDetails.BreachDate);
        this._breachAlertDate.hidden = isNaN(breachDate);
        if (!isNaN(breachDate)) {
          document.l10n.setAttributes(
            this._breachAlertDate,
            "about-logins-breach-alert-date",
            {
              date: breachDate.getTime(),
            }
          );
        }
      }
    }
    this._vulnerableAlert.hidden =
      !this._vulnerableLoginsMap ||
      !this._vulnerableLoginsMap.has(this._login.guid) ||
      !this._breachAlert.hidden;
    if (!this._vulnerableAlert.hidden) {
      this._vulnerableAlertLink.href = this._login.origin;
      document.l10n.setAttributes(
        this._vulnerableAlertLink,
        "about-logins-vulnerable-alert-link",
        {
          hostname: this._login.displayOrigin,
        }
      );
      this._vulnerableAlertLearnMoreLink.setAttribute(
        "href",
        window.AboutLoginsUtils.supportBaseURL + "lockwise-alerts"
      );
    }
    if (onlyUpdateErrorsAndAlerts) {
      return;
    }
    document.l10n.setAttributes(this._timeCreated, "login-item-time-created", {
      timeCreated: this._login.timeCreated || "",
    });
    document.l10n.setAttributes(this._timeChanged, "login-item-time-changed", {
      timeChanged: this._login.timePasswordChanged || "",
    });
    document.l10n.setAttributes(this._timeUsed, "login-item-time-used", {
      timeUsed: this._login.timeLastUsed || "",
    });

    if (this._login.faviconDataURI) {
      this._faviconWrapper.classList.add("hide-default-favicon");
      this._favicon.src = this._login.faviconDataURI;
      this._favicon.hidden = false;
    } else {
      // reset the src and alt attributes if the currently selected favicon doesn't have a favicon
      this._favicon.src = "";
      this._favicon.hidden = true;
      this._faviconWrapper.classList.remove("hide-default-favicon");
    }

    this._title.textContent = this._login.title;
    this._title.title = this._login.title;
    this._originInput.defaultValue = this._login.origin || "";
    if (this._login.origin) {
      // Creates anchor element with origin URL
      this._originDisplayInput.href = this._login.origin || "";
      this._originDisplayInput.innerText = this._login.origin || "";
    }
    this._usernameInput.defaultValue = this._login.username || "";
    if (this._login.password) {
      // We use .value instead of .defaultValue since the latter updates the
      // content attribute making the password easily viewable with Inspect
      // Element even when Master Password is enabled. This is only run when
      // the password is non-empty since setting the field to an empty value
      // would mark the field as 'dirty' for form validation and thus trigger
      // the error styling since the password field is 'required'.
      // This element is only in the document while unmasked or editing.
      this._passwordInput.value = this._login.password;

      // In masked non-edit mode we use a different "display" element to render
      // the masked password so that one cannot simply remove/change
      // @type=password to reveal the real password.
      this._passwordDisplayInput.value = " ".repeat(
        this._login.password.length
      );
    }

    if (this.dataset.editing) {
      this._usernameInput.removeAttribute("data-l10n-id");
      this._usernameInput.placeholder = "";
    } else {
      document.l10n.setAttributes(
        this._usernameInput,
        "about-logins-login-item-username"
      );
    }
    this._copyUsernameButton.disabled = !this._login.username;
    document.l10n.setAttributes(
      this._saveChangesButton,
      this.dataset.isNewLogin
        ? "login-item-save-new-button"
        : "login-item-save-changes-button"
    );
    this._updatePasswordRevealState();
    this._updateOriginDisplayState();
  }

  setBreaches(breachesByLoginGUID) {
    this._internalSetMonitorData("_breachesMap", breachesByLoginGUID);
  }

  updateBreaches(breachesByLoginGUID) {
    this._internalUpdateMonitorData("_breachesMap", breachesByLoginGUID);
  }

  setVulnerableLogins(vulnerableLoginsByLoginGUID) {
    this._internalSetMonitorData(
      "_vulnerableLoginsMap",
      vulnerableLoginsByLoginGUID
    );
  }

  updateVulnerableLogins(vulnerableLoginsByLoginGUID) {
    this._internalUpdateMonitorData(
      "_vulnerableLoginsMap",
      vulnerableLoginsByLoginGUID
    );
  }

  _internalSetMonitorData(internalMemberName, mapByLoginGUID) {
    this[internalMemberName] = mapByLoginGUID;
    this.render({ onlyUpdateErrorsAndAlerts: true });
  }

  _internalUpdateMonitorData(internalMemberName, mapByLoginGUID) {
    if (!this[internalMemberName]) {
      this[internalMemberName] = new Map();
    }
    for (const [guid, data] of [...mapByLoginGUID]) {
      if (data) {
        this[internalMemberName].set(guid, data);
      } else {
        this[internalMemberName].delete(guid);
      }
    }
    this._internalSetMonitorData(internalMemberName, this[internalMemberName]);
  }

  showLoginItemError(error) {
    this._error = error;
    this.render();
  }

  async handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsInitialLoginSelected": {
        this.setLogin(event.detail, { skipFocusChange: true });
        break;
      }
      case "AboutLoginsLoadInitialFavicon": {
        this.render();
        break;
      }
      case "AboutLoginsLoginSelected": {
        this.confirmPendingChangesOnEvent(event, event.detail);
        break;
      }
      case "AboutLoginsShowBlankLogin": {
        this.confirmPendingChangesOnEvent(event, {});
        break;
      }
      case "auxclick": {
        if (event.button == 1) {
          this._handleOriginClick();
        }
        break;
      }
      case "blur": {
        // Add https:// prefix if one was not provided.
        let originValue = this._originInput.value.trim();
        if (!originValue) {
          return;
        }
        if (!originValue.match(/:\/\//)) {
          this._originInput.value = "https://" + originValue;
        }
        break;
      }
      case "click": {
        let classList = event.currentTarget.classList;
        if (classList.contains("reveal-password-checkbox")) {
          // We prompt for the master password when entering edit mode already.
          if (this._revealCheckbox.checked && !this.dataset.editing) {
            let masterPasswordAuth = await promptForMasterPassword(
              "about-logins-reveal-password-os-auth-dialog-message"
            );
            if (!masterPasswordAuth) {
              this._revealCheckbox.checked = false;
              return;
            }
          }
          this._updatePasswordRevealState();

          let method = this._revealCheckbox.checked ? "show" : "hide";
          this._recordTelemetryEvent({ object: "password", method });
          return;
        }

        if (classList.contains("cancel-button")) {
          let wasExistingLogin = !!this._login.guid;
          if (wasExistingLogin) {
            if (this.hasPendingChanges()) {
              this.showConfirmationDialog("discard-changes", () => {
                this.setLogin(this._login);
              });
            } else {
              this.setLogin(this._login);
            }
          } else if (!this.hasPendingChanges()) {
            window.dispatchEvent(new CustomEvent("AboutLoginsClearSelection"));
            this._recordTelemetryEvent({
              object: "new_login",
              method: "cancel",
            });
          } else {
            this.showConfirmationDialog("discard-changes", () => {
              window.dispatchEvent(
                new CustomEvent("AboutLoginsClearSelection")
              );
            });
          }
          return;
        }
        if (
          classList.contains("copy-password-button") ||
          classList.contains("copy-username-button")
        ) {
          let copyButton = event.currentTarget;
          let otherCopyButton =
            copyButton == this._copyPasswordButton
              ? this._copyUsernameButton
              : this._copyPasswordButton;
          if (copyButton.dataset.copyLoginProperty == "password") {
            let masterPasswordAuth = await promptForMasterPassword(
              "about-logins-copy-password-os-auth-dialog-message"
            );
            if (!masterPasswordAuth) {
              return;
            }
          }

          copyButton.disabled = true;
          copyButton.dataset.copied = true;
          let propertyToCopy = this._login[
            copyButton.dataset.copyLoginProperty
          ];
          document.dispatchEvent(
            new CustomEvent("AboutLoginsCopyLoginDetail", {
              bubbles: true,
              detail: propertyToCopy,
            })
          );
          // If there is no username, this must be triggered by the password button,
          // don't enable otherCopyButton (username copy button) in this case.
          if (this._login.username) {
            otherCopyButton.disabled = false;
            delete otherCopyButton.dataset.copied;
          }
          clearTimeout(this._copyUsernameTimeoutId);
          clearTimeout(this._copyPasswordTimeoutId);
          let timeoutId = setTimeout(() => {
            copyButton.disabled = false;
            delete copyButton.dataset.copied;
          }, LoginItem.COPY_BUTTON_RESET_TIMEOUT);
          if (copyButton.dataset.copyLoginProperty == "password") {
            this._copyPasswordTimeoutId = timeoutId;
          } else {
            this._copyUsernameTimeoutId = timeoutId;
          }

          this._recordTelemetryEvent({
            object: copyButton.dataset.telemetryObject,
            method: "copy",
          });
          return;
        }
        if (classList.contains("delete-button")) {
          this.showConfirmationDialog("delete", () => {
            document.dispatchEvent(
              new CustomEvent("AboutLoginsDeleteLogin", {
                bubbles: true,
                detail: this._login,
              })
            );
          });
          return;
        }
        if (classList.contains("edit-button")) {
          let masterPasswordAuth = await promptForMasterPassword(
            "about-logins-edit-login-os-auth-dialog-message"
          );
          if (!masterPasswordAuth) {
            return;
          }

          this._toggleEditing();
          this.render();

          this._recordTelemetryEvent({
            object: "existing_login",
            method: "edit",
          });
          return;
        }
        if (
          event.target.dataset.l10nName == "duplicate-link" &&
          event.currentTarget.dataset.errorGuid
        ) {
          let existingDuplicateLogin = {
            guid: event.currentTarget.dataset.errorGuid,
          };
          window.dispatchEvent(
            new CustomEvent("AboutLoginsLoginSelected", {
              detail: existingDuplicateLogin,
              cancelable: true,
            })
          );
          return;
        }
        if (classList.contains("origin-input")) {
          this._handleOriginClick();
        }
        if (classList.contains("alert-learn-more-link")) {
          if (event.currentTarget.closest(".breach-alert")) {
            this._recordTelemetryEvent({
              object: "existing_login",
              method: "learn_more_breach",
            });
          } else if (event.currentTarget.closest(".vulnerable-alert")) {
            this._recordTelemetryEvent({
              object: "existing_login",
              method: "learn_more_vuln",
            });
          }
        }
        break;
      }
      case "submit": {
        // Prevent page navigation form submit behavior.
        event.preventDefault();
        if (!this._isFormValid({ reportErrors: true })) {
          return;
        }
        if (!this.hasPendingChanges()) {
          this._toggleEditing(false);
          this.render();
          return;
        }
        let loginUpdates = this._loginFromForm();
        if (this._login.guid) {
          loginUpdates.guid = this._login.guid;
          document.dispatchEvent(
            new CustomEvent("AboutLoginsUpdateLogin", {
              bubbles: true,
              detail: loginUpdates,
            })
          );

          this._recordTelemetryEvent({
            object: "existing_login",
            method: "save",
          });
        } else {
          document.dispatchEvent(
            new CustomEvent("AboutLoginsCreateLogin", {
              bubbles: true,
              detail: loginUpdates,
            })
          );

          this._recordTelemetryEvent({ object: "new_login", method: "save" });
        }
        break;
      }
      case "mousedown": {
        // No AutoScroll when middle clicking on origin input.
        if (event.currentTarget == this._originInput && event.button == 1) {
          event.preventDefault();
        }
        break;
      }
      case "AboutLoginsRemaskPassword": {
        if (this._revealCheckbox.checked && !this.dataset.editing) {
          this._revealCheckbox.checked = false;
        }
        this._updatePasswordRevealState();
        let method = this._revealCheckbox.checked ? "show" : "hide";
        this._recordTelemetryEvent({ object: "password", method });
        break;
      }
    }
  }

  /**
   * Helper to show the "Discard changes" confirmation dialog and delay the
   * received event after confirmation.
   * @param {object} event The event to be delayed.
   * @param {object} login The login to be shown on confirmation.
   */
  confirmPendingChangesOnEvent(event, login) {
    if (this.hasPendingChanges()) {
      event.preventDefault();
      this.showConfirmationDialog("discard-changes", () => {
        // Clear any pending changes
        this.setLogin(login);

        window.dispatchEvent(
          new CustomEvent(event.type, {
            detail: login,
            cancelable: false,
          })
        );
      });
    } else {
      this.setLogin(login);
    }
  }

  /**
   * Shows a confirmation dialog.
   * @param {string} type The type of confirmation dialog to display.
   * @param {boolean} onConfirm Optional, the function to execute when the confirm button is clicked.
   */
  showConfirmationDialog(type, onConfirm = () => {}) {
    const dialog = document.querySelector("confirmation-dialog");
    let options;
    switch (type) {
      case "delete": {
        options = {
          title: "about-logins-confirm-remove-dialog-title",
          message: "confirm-delete-dialog-message",
          confirmButtonLabel:
            "about-logins-confirm-remove-dialog-confirm-button",
        };
        break;
      }
      case "discard-changes": {
        options = {
          title: "confirm-discard-changes-dialog-title",
          message: "confirm-discard-changes-dialog-message",
          confirmButtonLabel: "confirm-discard-changes-dialog-confirm-button",
        };
        break;
      }
    }
    let wasExistingLogin = !!this._login.guid;
    let method = type == "delete" ? "delete" : "cancel";
    let dialogPromise = dialog.show(options);
    dialogPromise.then(
      () => {
        try {
          onConfirm();
        } catch (ex) {}
        this._recordTelemetryEvent({
          object: wasExistingLogin ? "existing_login" : "new_login",
          method,
        });
      },
      () => {}
    );
    return dialogPromise;
  }

  hasPendingChanges() {
    let valuesChanged = !window.AboutLoginsUtils.doLoginsMatch(
      Object.assign({ username: "", password: "", origin: "" }, this._login),
      this._loginFromForm()
    );

    return this.dataset.editing && valuesChanged;
  }

  resetForm() {
    // If the password input (which uses HTML form validation) wasn't connected,
    // append it to the form so it gets included in the reset, specifically for
    // .value and the dirty state for validation.
    let wasConnected = this._passwordInput.isConnected;
    if (!wasConnected) {
      this._revealCheckbox.insertAdjacentElement(
        "beforebegin",
        this._passwordInput
      );
    }

    this._form.reset();
    if (!wasConnected) {
      this._passwordInput.remove();
    }
  }

  /**
   * @param {login} login The login that should be displayed. The login object is
   *                      a plain JS object representation of nsILoginInfo/nsILoginMetaInfo.
   * @param {boolean} skipFocusChange Optional, if present and set to true, the Edit button of the
   *                                  login will not get focus automatically. This is used to prevent
   *                                  stealing focus from the search filter upon page load.
   */
  setLogin(login, { skipFocusChange } = {}) {
    this._login = login;
    this._error = null;

    this.resetForm();

    if (login.guid) {
      delete this.dataset.isNewLogin;
    } else {
      this.dataset.isNewLogin = true;
    }
    document.documentElement.classList.toggle("login-selected", login.guid);
    this._toggleEditing(!login.guid);

    this._revealCheckbox.checked = false;

    clearTimeout(this._copyUsernameTimeoutId);
    clearTimeout(this._copyPasswordTimeoutId);
    for (let copyButton of [
      this._copyUsernameButton,
      this._copyPasswordButton,
    ]) {
      copyButton.disabled = false;
      delete copyButton.dataset.copied;
    }

    if (!skipFocusChange) {
      this._editButton.focus();
    }
    this.render();
  }

  /**
   * Updates the view if the login argument matches the login currently
   * displayed.
   *
   * @param {login} login The login that was added to storage. The login object is
   *                      a plain JS object representation of nsILoginInfo/nsILoginMetaInfo.
   */
  loginAdded(login) {
    if (
      this._login.guid ||
      !window.AboutLoginsUtils.doLoginsMatch(login, this._loginFromForm())
    ) {
      return;
    }

    this.setLogin(login);
    this.dispatchEvent(
      new CustomEvent("AboutLoginsLoginSelected", {
        bubbles: true,
        composed: true,
        detail: login,
      })
    );
  }

  /**
   * Updates the view if the login argument matches the login currently
   * displayed.
   *
   * @param {login} login The login that was modified in storage. The login object is
   *                      a plain JS object representation of nsILoginInfo/nsILoginMetaInfo.
   */
  loginModified(login) {
    if (this._login.guid != login.guid) {
      return;
    }

    // Restore faviconDataURI on modified login
    if (this._login.faviconDataURI && this._login.origin == login.origin) {
      login.faviconDataURI = this._login.faviconDataURI;
    }

    let valuesChanged =
      this.dataset.editing &&
      !window.AboutLoginsUtils.doLoginsMatch(login, this._loginFromForm());
    if (valuesChanged) {
      this.showConfirmationDialog("discard-changes", () => {
        this.setLogin(login);
      });
    } else {
      this.setLogin(login);
    }
  }

  /**
   * Clears the displayed login if the argument matches the currently
   * displayed login.
   *
   * @param {login} login The login that was removed from storage. The login object is
   *                      a plain JS object representation of nsILoginInfo/nsILoginMetaInfo.
   */
  loginRemoved(login) {
    if (login.guid != this._login.guid) {
      return;
    }

    this.setLogin({}, { skipFocusChange: true });
    this._toggleEditing(false);
  }

  _handleOriginClick() {
    this._recordTelemetryEvent({
      object: "existing_login",
      method: "open_site",
    });
  }

  /**
   * Checks that the edit/new-login form has valid values present for their
   * respective required fields.
   *
   * @param {boolean} reportErrors If true, validation errors will be reported
   *                               to the user.
   */
  _isFormValid({ reportErrors } = {}) {
    let fields = [this._passwordInput];
    if (this.dataset.isNewLogin) {
      fields.push(this._originInput);
    }
    let valid = true;
    // Check validity on all required fields so each field will get :invalid styling
    // if applicable.
    for (let field of fields) {
      if (reportErrors) {
        valid &= field.reportValidity();
      } else {
        valid &= field.checkValidity();
      }
    }
    return valid;
  }

  _loginFromForm() {
    return Object.assign({}, this._login, {
      username: this._usernameInput.value.trim(),
      password: this._passwordInput.value,
      origin:
        window.AboutLoginsUtils.getLoginOrigin(this._originInput.value) || "",
    });
  }

  _recordTelemetryEvent(eventObject) {
    // Breach alerts have higher priority than vulnerable logins, the
    // following conditionals must reflect this priority.
    const extra = eventObject.hasOwnProperty("extra") ? eventObject.extra : {};
    if (this._breachesMap && this._breachesMap.has(this._login.guid)) {
      Object.assign(extra, { breached: "true" });
      eventObject.extra = extra;
    } else if (
      this._vulnerableLoginsMap &&
      this._vulnerableLoginsMap.has(this._login.guid)
    ) {
      Object.assign(extra, { vulnerable: "true" });
      eventObject.extra = extra;
    }
    recordTelemetryEvent(eventObject);
  }

  /**
   * Toggles the login-item view from editing to non-editing mode.
   *
   * @param {boolean} force When true puts the form in 'edit' mode, otherwise
   *                        puts the form in read-only mode.
   */
  _toggleEditing(force) {
    let shouldEdit = force !== undefined ? force : !this.dataset.editing;

    if (!shouldEdit) {
      delete this.dataset.isNewLogin;
    }

    // Reset cursor to the start of the input for long text names.
    this._usernameInput.scrollLeft = 0;

    if (shouldEdit) {
      this._passwordInput.style.removeProperty("width");
      this._passwordDisplayInput.style.removeProperty("width");
    } else {
      // Need to set a shorter width than -moz-available so the reveal checkbox
      // will still appear next to the password.
      this._passwordDisplayInput.style.width = this._passwordInput.style.width =
        (this._login.password || "").length + "ch";
    }

    this._deleteButton.disabled = this.dataset.isNewLogin;
    this._editButton.disabled = shouldEdit;
    let inputTabIndex = shouldEdit ? 0 : -1;
    this._originInput.readOnly = !this.dataset.isNewLogin;
    this._originInput.tabIndex = inputTabIndex;
    this._usernameInput.readOnly = !shouldEdit;
    this._usernameInput.tabIndex = inputTabIndex;
    this._passwordInput.readOnly = !shouldEdit;
    this._passwordInput.tabIndex = inputTabIndex;
    if (shouldEdit) {
      this.dataset.editing = true;
    } else {
      delete this.dataset.editing;
      // Only reset the reveal checkbox when exiting 'edit' mode
      this._revealCheckbox.checked = false;
    }
  }

  _updatePasswordRevealState() {
    if (
      window.AboutLoginsUtils &&
      window.AboutLoginsUtils.passwordRevealVisible === false
    ) {
      this._revealCheckbox.hidden = true;
    }

    let { checked } = this._revealCheckbox;
    let inputType = checked ? "text" : "password";
    this._passwordInput.type = inputType;

    // Swap which <input> is in the document depending on whether we need the
    // real .value (which means that the master password was already entered,
    // if applicable)
    if (checked || this.dataset.editing) {
      this._passwordDisplayInput.replaceWith(this._passwordInput);
    } else {
      this._passwordInput.replaceWith(this._passwordDisplayInput);
    }
  }

  _updateOriginDisplayState() {
    // Switches between the origin input and anchor tag depending
    // if a new login is being created.
    if (this.dataset.isNewLogin) {
      this._originDisplayInput.replaceWith(this._originInput);
      this._originInput.focus();
    } else {
      this._originInput.replaceWith(this._originDisplayInput);
    }
  }
}
customElements.define("login-item", LoginItem);
