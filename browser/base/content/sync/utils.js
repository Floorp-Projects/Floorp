/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Equivalent to 0o600 permissions; used for saved Sync Recovery Key.
// This constant can be replaced when the equivalent values are available to
// chrome JS; see Bug 433295 and Bug 757351.
const PERMISSIONS_RWUSR = 0x180;

// Weave should always exist before before this file gets included.
var gSyncUtils = {
  get bundle() {
    delete this.bundle;
    return this.bundle = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
  },

  get fxAccountsEnabled() {
    let service = Components.classes["@mozilla.org/weave/service;1"]
                            .getService(Components.interfaces.nsISupports)
                            .wrappedJSObject;
    return service.fxAccountsEnabled;
  },

  // opens in a new window if we're in a modal prefwindow world, in a new tab otherwise
  _openLink(url) {
    let thisDocEl = document.documentElement,
        openerDocEl = window.opener && window.opener.document.documentElement;
    if (thisDocEl.id == "accountSetup" && window.opener &&
        openerDocEl.id == "BrowserPreferences" && !openerDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (thisDocEl.id == "BrowserPreferences" && !thisDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (document.documentElement.id == "change-dialog")
      Services.wm.getMostRecentWindow("navigator:browser")
              .openUILinkIn(url, "tab");
    else
      openUILinkIn(url, "tab");
  },

  changeName: function changeName(input) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Service.clientsEngine.localName = input.value;
    input.value = Weave.Service.clientsEngine.localName;
  },

  openChange: function openChange(type, duringSetup) {
    // Just re-show the dialog if it's already open
    let openedDialog = Services.wm.getMostRecentWindow("Sync:" + type);
    if (openedDialog != null) {
      openedDialog.focus();
      return;
    }

    // Open up the change dialog
    let changeXUL = "chrome://browser/content/sync/genericChange.xul";
    let changeOpt = "centerscreen,chrome,resizable=no";
    Services.ww.activeWindow.openDialog(changeXUL, "", changeOpt,
                                        type, duringSetup);
  },

  changePassword() {
    if (Weave.Utils.ensureMPUnlocked())
      this.openChange("ChangePassword");
  },

  resetPassphrase(duringSetup) {
    if (Weave.Utils.ensureMPUnlocked())
      this.openChange("ResetPassphrase", duringSetup);
  },

  updatePassphrase() {
    if (Weave.Utils.ensureMPUnlocked())
      this.openChange("UpdatePassphrase");
  },

  resetPassword() {
    this._openLink(Weave.Service.pwResetURL);
  },

  get tosURL() {
    let root = this.fxAccountsEnabled ? "fxa." : "";
    return Weave.Svc.Prefs.get(root + "termsURL");
  },

  openToS() {
    this._openLink(this.tosURL);
  },

  get privacyPolicyURL() {
    let root = this.fxAccountsEnabled ? "fxa." : "";
    return Weave.Svc.Prefs.get(root + "privacyURL");
  },

  openPrivacyPolicy() {
    this._openLink(this.privacyPolicyURL);
  },

  /**
   * Prepare an invisible iframe with the passphrase backup document.
   * Used by both the print and saving methods.
   *
   * @param elid : ID of the form element containing the passphrase.
   * @param callback : Function called once the iframe has loaded.
   */
  _preparePPiframe(elid, callback) {
    let pp = document.getElementById(elid).value;

    // Create an invisible iframe whose contents we can print.
    let iframe = document.createElement("iframe");
    iframe.setAttribute("src", "chrome://browser/content/sync/key.xhtml");
    iframe.collapsed = true;
    document.documentElement.appendChild(iframe);
    iframe.contentWindow.addEventListener("load", function() {
      iframe.contentWindow.removeEventListener("load", arguments.callee, false);

      // Insert the Sync Key into the page.
      let el = iframe.contentDocument.getElementById("synckey");
      el.firstChild.nodeValue = pp;

      // Insert the TOS and Privacy Policy URLs into the page.
      let termsURL = Weave.Svc.Prefs.get("termsURL");
      el = iframe.contentDocument.getElementById("tosLink");
      el.setAttribute("href", termsURL);
      el.firstChild.nodeValue = termsURL;

      let privacyURL = Weave.Svc.Prefs.get("privacyURL");
      el = iframe.contentDocument.getElementById("ppLink");
      el.setAttribute("href", privacyURL);
      el.firstChild.nodeValue = privacyURL;

      callback(iframe);
    }, false);
  },

  /**
   * Print passphrase backup document.
   *
   * @param elid : ID of the form element containing the passphrase.
   */
  passphrasePrint(elid) {
    this._preparePPiframe(elid, function(iframe) {
      let webBrowserPrint = iframe.contentWindow
                                  .QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebBrowserPrint);
      let printSettings = PrintUtils.getPrintSettings();

      // Display no header/footer decoration except for the date.
      printSettings.headerStrLeft
        = printSettings.headerStrCenter
        = printSettings.headerStrRight
        = printSettings.footerStrLeft
        = printSettings.footerStrCenter = "";
      printSettings.footerStrRight = "&D";

      try {
        webBrowserPrint.print(printSettings, null);
      } catch (ex) {
        // print()'s return codes are expressed as exceptions. Ignore.
      }
    });
  },

  /**
   * Save passphrase backup document to disk as HTML file.
   *
   * @param elid : ID of the form element containing the passphrase.
   */
  passphraseSave(elid) {
    let dialogTitle = this.bundle.GetStringFromName("save.recoverykey.title");
    let defaultSaveName = this.bundle.GetStringFromName("save.recoverykey.defaultfilename");
    this._preparePPiframe(elid, function(iframe) {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      let fpCallback = function fpCallback_done(aResult) {
        if (aResult == Ci.nsIFilePicker.returnOK ||
            aResult == Ci.nsIFilePicker.returnReplace) {
          let stream = Cc["@mozilla.org/network/file-output-stream;1"].
                       createInstance(Ci.nsIFileOutputStream);
          stream.init(fp.file, -1, PERMISSIONS_RWUSR, 0);

          let serializer = new XMLSerializer();
          let output = serializer.serializeToString(iframe.contentDocument);
          output = output.replace(/<!DOCTYPE (.|\n)*?]>/,
            '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" ' +
            '"DTD/xhtml1-strict.dtd">');
          output = Weave.Utils.encodeUTF8(output);
          stream.write(output, output.length);
        }
      };

      fp.init(window, dialogTitle, Ci.nsIFilePicker.modeSave);
      fp.appendFilters(Ci.nsIFilePicker.filterHTML);
      fp.defaultString = defaultSaveName;
      fp.open(fpCallback);
      return false;
    });
  },

  /**
   * validatePassword
   *
   * @param el1 : the first textbox element in the form
   * @param el2 : the second textbox element, if omitted it's an update form
   *
   * returns [valid, errorString]
   */
  validatePassword(el1, el2) {
    let valid = false;
    let val1 = el1.value;
    let val2 = el2 ? el2.value : "";
    let error = "";

    if (!el2)
      valid = val1.length >= Weave.MIN_PASS_LENGTH;
    else if (val1 && val1 == Weave.Service.identity.username)
      error = "change.password.pwSameAsUsername";
    else if (val1 && val1 == Weave.Service.identity.account)
      error = "change.password.pwSameAsEmail";
    else if (val1 && val1 == Weave.Service.identity.basicPassword)
      error = "change.password.pwSameAsPassword";
    else if (val1 && val2) {
      if (val1 == val2 && val1.length >= Weave.MIN_PASS_LENGTH)
        valid = true;
      else if (val1.length < Weave.MIN_PASS_LENGTH)
        error = "change.password.tooShort";
      else if (val1 != val2)
        error = "change.password.mismatch";
    }
    let errorString = error ? Weave.Utils.getErrorString(error) : "";
    return [valid, errorString];
  }
};
