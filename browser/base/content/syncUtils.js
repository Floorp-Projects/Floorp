/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
 *   Mike Connor <mconnor@mozilla.com>
 *   Paul O’Shannessy <paul@oshannessy.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Weave should always exist before before this file gets included.
let gSyncUtils = {
  get bundle() {
    delete this.bundle;
    return this.bundle = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
  },

  // opens in a new window if we're in a modal prefwindow world, in a new tab otherwise
  _openLink: function (url) {
    let thisDocEl = document.documentElement,
        openerDocEl = window.opener && window.opener.document.documentElement;
    if (thisDocEl.id == "accountSetup" && window.opener &&
        openerDocEl.id == "BrowserPreferences" && !openerDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (thisDocEl.id == "BrowserPreferences" && !thisDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (document.documentElement.id == "change-dialog")
      Weave.Svc.WinMediator.getMostRecentWindow("navigator:browser")
        .openUILinkIn(url, "tab");
    else
      openUILinkIn(url, "tab");
  },

  changeName: function changeName(input) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Clients.localName = input.value;
    input.value = Weave.Clients.localName;
  },

  openChange: function openChange(type) {
    // Just re-show the dialog if it's already open
    let openedDialog = Weave.Svc.WinMediator.getMostRecentWindow("Sync:" + type);
    if (openedDialog != null) {
      openedDialog.focus();
      return;
    }

    // Open up the change dialog
    let changeXUL = "chrome://browser/content/syncGenericChange.xul";
    let changeOpt = "centerscreen,chrome,resizable=no";
    Weave.Svc.WinWatcher.activeWindow.openDialog(changeXUL, "", changeOpt, type);
  },

  changePassword: function () {
    if (Weave.Utils.ensureMPUnlocked())
      this.openChange("ChangePassword");
  },

  resetPassphrase: function () {
    if (Weave.Utils.ensureMPUnlocked())
      this.openChange("ResetPassphrase");
  },

  updatePassphrase: function () {
    if (Weave.Utils.ensureMPUnlocked())
      this.openChange("UpdatePassphrase");
  },

  resetPassword: function () {
    this._openLink(Weave.Service.pwResetURL);
  },

  openToS: function () {
    this._openLink(Weave.Svc.Prefs.get("termsURL"));
  },

  openPrivacyPolicy: function () {
    this._openLink(Weave.Svc.Prefs.get("privacyURL"));
  },

  // xxxmpc - fix domain before 1.3 final (bug 583652)
  _baseURL: "http://www.mozilla.com/firefox/sync/",

  openFirstClientFirstrun: function () {
    let url = this._baseURL + "firstrun.html";
    this._openLink(url);
  },

  openAddedClientFirstrun: function () {
    let url = this._baseURL + "secondrun.html";
    this._openLink(url);
  },

  /**
   * Prepare an invisible iframe with the passphrase backup document.
   * Used by both the print and saving methods.
   *
   * @param elid : ID of the form element containing the passphrase.
   * @param callback : Function called once the iframe has loaded.
   */
  _preparePPiframe: function(elid, callback) {
    let pp = document.getElementById(elid).value;

    // Create an invisible iframe whose contents we can print.
    let iframe = document.createElement("iframe");
    iframe.setAttribute("src", "chrome://browser/content/syncKey.xhtml");
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
  passphrasePrint: function(elid) {
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
  passphraseSave: function(elid) {
    let dialogTitle = this.bundle.GetStringFromName("save.synckey.title");
    this._preparePPiframe(elid, function(iframe) {
      let filepicker = Cc["@mozilla.org/filepicker;1"]
                         .createInstance(Ci.nsIFilePicker);
      filepicker.init(window, dialogTitle, Ci.nsIFilePicker.modeSave);
      filepicker.appendFilters(Ci.nsIFilePicker.filterHTML);
      filepicker.defaultString = "Firefox Sync Key.html";
      let rv = filepicker.show();
      if (rv == Ci.nsIFilePicker.returnOK
          || rv == Ci.nsIFilePicker.returnReplace) {
        let stream = Cc["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Ci.nsIFileOutputStream);
        stream.init(filepicker.file, -1, -1, 0);

        let serializer = new XMLSerializer();
        let output = serializer.serializeToString(iframe.contentDocument);
        output = output.replace(/<!DOCTYPE (.|\n)*?]>/,
          '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" ' +
          '"DTD/xhtml1-strict.dtd">');
        output = Weave.Utils.encodeUTF8(output);
        stream.write(output, output.length);
      }
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
  validatePassword: function (el1, el2) {
    let valid = false;
    let val1 = el1.value;
    let val2 = el2 ? el2.value : "";
    let error = "";

    if (!el2)
      valid = val1.length >= Weave.MIN_PASS_LENGTH;
    else if (val1 && val1 == Weave.Service.username)
      error = "change.password.pwSameAsUsername";
    else if (val1 && val1 == Weave.Service.account)
      error = "change.password.pwSameAsEmail";
    else if (val1 && val1 == Weave.Service.password)
      error = "change.password.pwSameAsPassword";
    else if (val1 && val1 == Weave.Service.passphrase)
      error = "change.password.pwSameAsSyncKey";
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
