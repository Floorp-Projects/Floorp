/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { recordTelemetryEvent } from "../aboutLoginsUtils.js";

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
    this._form = this.shadowRoot.querySelector("form");
    this._originInput = this.shadowRoot.querySelector("input[name='origin']");
    this._usernameInput = this.shadowRoot.querySelector(
      "input[name='username']"
    );
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
    this._dismissBreachAlert = this.shadowRoot.querySelector(
      ".dismiss-breach-alert"
    );

    this.render();

    this._originInput.addEventListener("blur", this);
    this._cancelButton.addEventListener("click", this);
    this._copyPasswordButton.addEventListener("click", this);
    this._copyUsernameButton.addEventListener("click", this);
    this._deleteButton.addEventListener("click", this);
    this._dismissBreachAlert.addEventListener("click", this);
    this._editButton.addEventListener("click", this);
    this._form.addEventListener("submit", this);
    this._originInput.addEventListener("click", this);
    this._revealCheckbox.addEventListener("click", this);
    window.addEventListener("AboutLoginsInitialLoginSelected", this);
    window.addEventListener("AboutLoginsLoadInitialFavicon", this);
    window.addEventListener("AboutLoginsLoginSelected", this);
    window.addEventListener("AboutLoginsShowBlankLogin", this);
  }

  async render() {
    this._breachAlert.hidden = true;
    if (this._breachesMap && this._breachesMap.has(this._login.guid)) {
      const breachDetails = this._breachesMap.get(this._login.guid);
      const breachAlertLink = this._breachAlert.querySelector(
        ".breach-alert-link"
      );
      breachAlertLink.href = breachDetails.breachAlertURL;
      document.l10n.setAttributes(
        this._dismissBreachAlert,
        "breach-alert-dismiss"
      );
      this._breachAlert.hidden = false;
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
      document.l10n.setAttributes(this._favicon, "login-favicon", {
        title: this._login.title,
      });
      this._favicon.hidden = false;
    } else {
      // reset the src and alt attributes if the currently selected favicon doesn't have a favicon
      this._favicon.hidden = true;
      this._faviconWrapper.classList.remove("hide-default-favicon");
    }

    this._title.textContent = this._login.title;
    this._originInput.defaultValue = this._login.origin || "";
    this._usernameInput.defaultValue = this._login.username || "";
    this._passwordInput.defaultValue = this._login.password || "";
    document.l10n.setAttributes(
      this._saveChangesButton,
      this.dataset.isNewLogin
        ? "login-item-save-new-button"
        : "login-item-save-changes-button"
    );
    await this._updatePasswordRevealState();
  }

  updateBreaches(breachesByLoginGUID) {
    this._breachesMap = breachesByLoginGUID;
    this.render();
  }

  dismissBreachAlert() {
    document.dispatchEvent(
      new CustomEvent("AboutLoginsDismissBreachAlert", {
        bubbles: true,
        detail: this._login,
      })
    );
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
          await this._updatePasswordRevealState();

          let method = this._revealCheckbox.checked ? "show" : "hide";
          recordTelemetryEvent({ object: "password", method });
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
            recordTelemetryEvent({
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
          if (copyButton.dataset.copyLoginProperty == "password") {
            let masterPasswordAuth = await new Promise(resolve => {
              window.AboutLoginsUtils.promptForMasterPassword(resolve);
            });
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
          let timeoutId = setTimeout(() => {
            copyButton.disabled = false;
            delete copyButton.dataset.copied;
          }, LoginItem.COPY_BUTTON_RESET_TIMEOUT);
          if (copyButton.dataset.copyLoginProperty == "password") {
            this._copyPasswordTimeoutId = timeoutId;
          } else {
            this._copyUsernameTimeoutId = timeoutId;
          }

          recordTelemetryEvent({
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
        if (classList.contains("dismiss-breach-alert")) {
          this.dismissBreachAlert();
          return;
        }
        if (classList.contains("edit-button")) {
          this._toggleEditing();

          recordTelemetryEvent({ object: "existing_login", method: "edit" });
          return;
        }
        if (classList.contains("origin-input") && !this.readOnly) {
          document.dispatchEvent(
            new CustomEvent("AboutLoginsOpenSite", {
              bubbles: true,
              detail: this._login,
            })
          );

          recordTelemetryEvent({
            object: "existing_login",
            method: "open_site",
          });
        }
        break;
      }
      case "submit": {
        // Prevent page navigation form submit behavior.
        event.preventDefault();
        if (!this._isFormValid({ reportErrors: true })) {
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

          recordTelemetryEvent({ object: "existing_login", method: "save" });
        } else {
          document.dispatchEvent(
            new CustomEvent("AboutLoginsCreateLogin", {
              bubbles: true,
              detail: loginUpdates,
            })
          );

          recordTelemetryEvent({ object: "new_login", method: "save" });
        }
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
          title: "confirm-delete-dialog-title",
          message: "confirm-delete-dialog-message",
          confirmButtonLabel: "confirm-delete-dialog-confirm-button",
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
        recordTelemetryEvent({
          object: wasExistingLogin ? "existing_login" : "new_login",
          method,
        });
      },
      () => {}
    );
    return dialogPromise;
  }

  hasPendingChanges() {
    let { origin = "", username = "", password = "" } = this._login || {};

    let valuesChanged = !window.AboutLoginsUtils.doLoginsMatch(
      { origin, username, password },
      this._loginFromForm()
    );

    return this.dataset.editing && valuesChanged;
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

    this._form.reset();

    if (login.guid) {
      delete this.dataset.isNewLogin;
    } else {
      this.dataset.isNewLogin = true;
    }
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

    this._login = login;
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

    this._login = login;
    this._toggleEditing(false);
    this.render();
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

    this._login = {};
    this._toggleEditing(false);
    this.render();
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
    return {
      username: this._usernameInput.value.trim(),
      password: this._passwordInput.value,
      origin:
        window.AboutLoginsUtils.getLoginOrigin(this._originInput.value) || "",
    };
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

    if (shouldEdit) {
      this._passwordInput.style.removeProperty("width");
    } else {
      // Need to set a shorter width than -moz-available so the reveal checkbox
      // will still appear next to the password.
      this._passwordInput.style.width =
        (this._login.password || "").length + "ch";
    }

    this._deleteButton.disabled = this.dataset.isNewLogin;
    this._editButton.disabled = shouldEdit;
    let inputTabIndex = !shouldEdit ? -1 : 0;
    this._originInput.readOnly = !this.dataset.isNewLogin;
    this._originInput.tabIndex = inputTabIndex;
    this._usernameInput.readOnly = !shouldEdit;
    this._usernameInput.tabIndex = inputTabIndex;
    this._passwordInput.readOnly = !shouldEdit;
    this._passwordInput.tabIndex = inputTabIndex;
    if (shouldEdit) {
      this.dataset.editing = true;
      this._originInput.focus();
    } else {
      delete this.dataset.editing;
    }
  }

  async _updatePasswordRevealState() {
    if (this._revealCheckbox.checked) {
      let masterPasswordAuth = await new Promise(resolve => {
        window.AboutLoginsUtils.promptForMasterPassword(resolve);
      });
      if (!masterPasswordAuth) {
        this._revealCheckbox.checked = false;
        return;
      }
    }

    let titleId = this._revealCheckbox.checked
      ? "login-item-password-reveal-checkbox-hide"
      : "login-item-password-reveal-checkbox-show";
    document.l10n.setAttributes(this._revealCheckbox, titleId);

    let { checked } = this._revealCheckbox;
    let inputType = checked ? "text" : "password";
    this._passwordInput.setAttribute("type", inputType);
  }
}
customElements.define("login-item", LoginItem);
