# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Asaf Romano <mozilla.mano@sent.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gGeneralPane = {
  _pane: null,

  setHomePageToCurrentPage: function ()
  {
    var win;
    if (document.documentElement.instantApply) {
      // If we're in instant-apply mode, use the most recent browser window
      var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                         .getService(Components.interfaces.nsIWindowMediator);
      win = wm.getMostRecentWindow("navigator:browser");
    }
    else
      win = window.opener;

    if (win) {
      var homePageField = document.getElementById("browserStartupHomepage");
      var newVal = "";

      var tabbrowser = win.document.getElementById("content");
      var l = tabbrowser.browsers.length;
      for (var i = 0; i < l; i++) {
        if (i)
          newVal += "|";
        newVal += tabbrowser.getBrowserAtIndex(i).webNavigation.currentURI.spec;
      }
      
      homePageField.value = newVal;
      this._pane.userChangedValue(homePageField);
    }
  },
  
  setHomePageToBookmark: function ()
  {
    var rv = { url: null };
    document.documentElement.openSubDialog("chrome://browser/content/bookmarks/selectBookmark.xul",
                                           "resizable", rv);  
    if (rv.url) {
      var homePageField = document.getElementById("browserStartupHomepage");
      homePageField.value = rv.url;
      this._pane.userChangedValue(homePageField);
    }
  },
  
  setHomePageToDefaultPage: function ()
  {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    var pref = prefService.getDefaultBranch(null);
    var url = pref.getComplexValue("browser.startup.homepage",
                                   Components.interfaces.nsIPrefLocalizedString).data;
    var homePageField = document.getElementById("browserStartupHomepage");
    homePageField.value = url;
    
    this._pane.userChangedValue(homePageField);
  },
  
  setHomePageToBlankPage: function ()
  {
    var homePageField = document.getElementById("browserStartupHomepage");
    homePageField.value = "about:blank";
    
    this._pane.userChangedValue(homePageField);
  },
  
  // Update the Home Button tooltip on all open browser windows.
  homepageChanged: function (aEvent)
  {
    var homepage = aEvent.target.value;
    // Replace pipes with commas to look nicer.
    homepage = homepage.replace(/\|/g,', ');
    
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                      .getService(Components.interfaces.nsIWindowMediator);
    var e = wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      var win = e.getNext();
      if (!(win instanceof Components.interfaces.nsIDOMWindow))
        break;
      var homeButton = win.document.getElementById("home-button");
      if (homeButton)
        homeButton.setAttribute("tooltiptext", homepage);
    }
  },
  
  init: function ()
  {
    this._pane = document.getElementById("paneGeneral");

    this._updateUseCurrentButton();
    if (document.documentElement.instantApply)
      window.addEventListener("focus", this._updateUseCurrentButton, false);
  },

  _updateUseCurrentButton: function () {
    var useButton = document.getElementById("browserUseCurrent");

    var win;
    if (document.documentElement.instantApply) {
      // If we're in instant-apply mode, use the most recent browser window
      var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                         .getService(Components.interfaces.nsIWindowMediator);
      win = wm.getMostRecentWindow("navigator:browser");
    }
    else
      win = window.opener;

    if (win && win.document.documentElement
                  .getAttribute("windowtype") == "navigator:browser") {
      useButton.disabled = false;

      var tabbrowser = win.document.getElementById("content");  
      if (tabbrowser.browsers.length > 1)
        useButton.label = useButton.getAttribute("label2");
    }
    else {
      useButton.disabled = true;
    }
  },

  showConnections: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/connection.xul",
                                           "", null);
  },

#ifdef HAVE_SHELL_SERVICE
  checkNow: function ()
  {
    var shellSvc = Components.classes["@mozilla.org/browser/shell-service;1"]
                             .getService(Components.interfaces.nsIShellService);
    var brandBundle = document.getElementById("bundleBrand");
    var shellBundle = document.getElementById("bundleShell");
    var brandShortName = brandBundle.getString("brandShortName");
    var promptTitle = shellBundle.getString("setDefaultBrowserTitle");
    var promptMessage;
    const IPS = Components.interfaces.nsIPromptService;
    var psvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                         .getService(IPS);
    if (!shellSvc.isDefaultBrowser(false)) {
      promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage", 
                                                     [brandShortName]);
      var rv = psvc.confirmEx(window, promptTitle, promptMessage, 
                              (IPS.BUTTON_TITLE_YES * IPS.BUTTON_POS_0) + 
                              (IPS.BUTTON_TITLE_NO * IPS.BUTTON_POS_1),
                              null, null, null, null, { });
      if (rv == 0)
        shellSvc.setDefaultBrowser(true, false);
    }
    else {
      promptMessage = shellBundle.getFormattedString("alreadyDefaultBrowser",
                                                     [brandShortName]);
      psvc.alert(window, promptTitle, promptMessage);
    }
  }
#endif
};
