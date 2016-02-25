/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test adding a certificate exception by attempting to browse to a site with
// a bad certificate, being redirected to the internal about:certerror page,
// using the button contained therein to load the certificate exception
// dialog, using that to add an exception, and finally successfully visiting
// the site, including showing the right identity box and control center icons.
function test() {
  waitForExplicitFinish();
  whenNewTabLoaded(window, loadBadCertPage);
}

// Attempt to load https://expired.example.com (which has an expired cert).
function loadBadCertPage() {
  gBrowser.addProgressListener(certErrorProgressListener);
  gBrowser.selectedBrowser.loadURI("https://expired.example.com");
}

// The browser should load about:certerror. When This happens, click the
// button to open the certificate exception dialog.
var certErrorProgressListener = {
  buttonClicked: false,

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      let self = this;
      // Can't directly call button.click() in onStateChange
      executeSoon(function() {
        let button = content.document.getElementById("exceptionDialogButton");
        // If about:certerror hasn't fully loaded, the button won't be present.
        // It will eventually be there, however.
        if (button && !self.buttonClicked) {
          gBrowser.removeProgressListener(self);
          Services.obs.addObserver(certExceptionDialogObserver,
                                   "cert-exception-ui-ready", false);
          button.click();
        }
      });
    }
  }
};

// When the certificate exception dialog has opened, click the button to add
// an exception.
const EXCEPTION_DIALOG_URI = "chrome://pippki/content/exceptionDialog.xul";
var certExceptionDialogObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "cert-exception-ui-ready") {
      Services.obs.removeObserver(this, "cert-exception-ui-ready");
      let certExceptionDialog = getDialog(EXCEPTION_DIALOG_URI);
      ok(certExceptionDialog, "found exception dialog");
      executeSoon(function() {
        gBrowser.selectedBrowser.addEventListener("load",
                                                  successfulLoadListener,
                                                  true);
        certExceptionDialog.documentElement.getButton("extra1").click();
      });
    }
  }
};

// Finally, we should successfully load https://expired.example.com.
var successfulLoadListener = {
  handleEvent: function() {
    gBrowser.selectedBrowser.removeEventListener("load", this, true);
    checkControlPanelIcons();
    let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                                .getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride("expired.example.com", -1);
    gBrowser.removeTab(gBrowser.selectedTab);
    finish();
  }
};

// Check for the correct icons in the identity box and control center.
function checkControlPanelIcons() {
  let { gIdentityHandler } = gBrowser.ownerGlobal;
  gIdentityHandler._identityBox.click();
  document.getElementById("identity-popup-security-expander").click();

  is_element_visible(document.getElementById("connection-icon"));
  let connectionIconImage = gBrowser.ownerGlobal
        .getComputedStyle(document.getElementById("connection-icon"), "")
        .getPropertyValue("list-style-image");
  let securityViewBG = gBrowser.ownerGlobal
        .getComputedStyle(document.getElementById("identity-popup-securityView"), "")
        .getPropertyValue("background-image");
  let securityContentBG = gBrowser.ownerGlobal
        .getComputedStyle(document.getElementById("identity-popup-security-content"), "")
        .getPropertyValue("background-image");
  is(connectionIconImage,
     "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
     "Using expected icon image in the identity block");
  is(securityViewBG,
     "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
     "Using expected icon image in the Control Center main view");
  is(securityContentBG,
     "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
     "Using expected icon image in the Control Center subview");

  gIdentityHandler._identityPopup.hidden = true;
}

// Utility function to get a handle on the certificate exception dialog.
// Modified from toolkit/components/passwordmgr/test/prompt_common.js
function getDialog(aLocation) {
  let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
             .getService(Ci.nsIWindowMediator);
  let enumerator = wm.getXULWindowEnumerator(null);

  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
    let windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;

    let containedDocShells = windowDocShell.getDocShellEnumerator(
                                      Ci.nsIDocShellTreeItem.typeChrome,
                                      Ci.nsIDocShell.ENUMERATE_FORWARDS);
    while (containedDocShells.hasMoreElements()) {
      // Get the corresponding document for this docshell
      let childDocShell = containedDocShells.getNext();
      let childDoc = childDocShell.QueryInterface(Ci.nsIDocShell)
                                  .contentViewer
                                  .DOMDocument;

      if (childDoc.location.href == aLocation) {
        return childDoc;
      }
    }
  }
}
