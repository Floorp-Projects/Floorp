/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PromptHelper = {
  closeDialog: function(confirm, id) {
    let dialog = document.getElementById(id);
    if (typeof confirm == "boolean" && dialog.arguments && "defaultButton" in dialog.arguments)
      // confirmEx always returns 1 when dismissed with "escape" (bug 345067).
      dialog.arguments.result = confirm ? dialog.arguments.defaultButton : 1;
    else
      dialog.arguments.result = confirm;
    dialog.close();
  },

  // Alert dialog
  onCloseAlert: function(dialog) {
    if (dialog.arguments)
      dialog.arguments.value = document.getElementById("prompt-alert-checkbox").checked;
  },

  // Confirm dialog
  closeConfirm: function(confirm) {
    this.closeDialog(confirm, "prompt-confirm-dialog");
  },

  onCloseConfirm: function(dialog) {
    if (dialog.arguments && ("checkbox" in dialog.arguments))
      dialog.arguments.checkbox.value = document.getElementById("prompt-confirm-checkbox").checked;
  },

  // Prompt dialog
  closePrompt: function(confirm) {
    this.closeDialog(confirm, "prompt-prompt-dialog");
  },

  onClosePrompt: function(dialog) {
    if (dialog.arguments) {
      dialog.arguments.checkbox.value = document.getElementById("prompt-prompt-checkbox").checked;
      dialog.arguments.value.value = document.getElementById("prompt-prompt-textbox").value;
    }
  },

  // User / Password dialog
  onLoadPassword: function onLoadPassword(dialog) {
    let user = document.getElementById('prompt-password-user');
    if (!user.value)
      user.focus();
  },

  closePassword: function(confirm) {
    this.closeDialog(confirm, "prompt-password-dialog");
  },

  onClosePassword: function(dialog) {
    if (dialog.arguments) {
      dialog.arguments.checkbox.value = document.getElementById("prompt-password-checkbox").checked;
      dialog.arguments.user.value = document.getElementById("prompt-password-user").value;
      dialog.arguments.password.value = document.getElementById("prompt-password-password").value;
    }
  },

  // Select dialog
  closeSelect: function(confirm) {
    this.closeDialog(confirm, "prompt-select-dialog");
  },

  onCloseSelect: function(dialog) {
    if (dialog.arguments)
      dialog.arguments.selection.value = document.getElementById("prompt-select-list").selectedIndex;
  }
};

var CrashPrompt = {
  load: function () {
    if (this._preventRecurse) {
      throw new Exception("CrashPrompt recursion error!!");
      return;
    }
    // populate the text blurb with localized text
    let brandName = Services.strings.createBundle("chrome://branding/locale/brand.properties")
                                    .GetStringFromName("brandShortName");
    let vendorName = Services.strings.createBundle("chrome://branding/locale/brand.properties")
                                     .GetStringFromName("vendorShortName");
    let crashBundle = Services.strings.createBundle("chrome://browser/locale/crashprompt.properties");
    let message = crashBundle.formatStringFromName("crashprompt.messagebody", [brandName, vendorName, brandName], 3);
    let descElement = document.getElementById("privacy-crash-blurb");
    descElement.textContent = message;

    // focus the send button
    document.getElementById('crash-button-accept').focus();
  },

  accept: function() {
    document.getElementById("crash-prompt-dialog").close();
    Services.prefs.setBoolPref('app.crashreporter.autosubmit', true);
    Services.prefs.setBoolPref('app.crashreporter.prompted', true);
    BrowserUI.crashReportingPrefChanged(true);

    this._preventRecurse = true;
    BrowserUI.startupCrashCheck();
    this._preventRecurse = false;
  },

  refuse: function() {
    document.getElementById("crash-prompt-dialog").close();
    Services.prefs.setBoolPref('app.crashreporter.autosubmit', false);
    Services.prefs.setBoolPref('app.crashreporter.prompted', true);
    BrowserUI.crashReportingPrefChanged(false);
  },

  privacy: function() {
    document.getElementById("crash-privacy-options").setAttribute("hidden", true);
    document.getElementById("crash-privacy-statement").setAttribute("hidden", false);
  },

  privacyBack: function() {
    document.getElementById("crash-privacy-options").setAttribute("hidden", false);
    document.getElementById("crash-privacy-statement").setAttribute("hidden", true);
  },
};
