/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIPrefService    = Components.interfaces.nsIPrefService;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;

const PREFSERVICE_CONTRACTID    = "@mozilla.org/preferences-service;1";

function setHomePageToCurrentPage()
{
  if (top.opener) {
    var homePageField = document.getElementById("browserStartupHomepage");
    var newVal = "";

    var browser = top.opener.document.getElementById("content");
    var l = browser.mPanelContainer.childNodes.length;
    for (var i = 0; i < l; i++) {
      if (i)
        newVal += "|";
      newVal += browser.mPanelContainer.childNodes[i].webNavigation.currentURI.spec;
    }
    
    homePageField.value = newVal;
  }
}

function setHomePageToBookmark()
{
  var rv = { url: null };
  openDialog("chrome://browser/content/bookmarks/selectBookmark.xul", "", 
             "centerscreen,chrome,modal=yes,dialog=yes,resizable=yes", rv);
  if (rv.url) {
    var homePageField = document.getElementById("browserStartupHomepage");
    homePageField.value = rv.url;
  }
}

function setHomePageToDefaultPage()
{
  var prefService = Components.classes[PREFSERVICE_CONTRACTID]
                              .getService(nsIPrefService);
  var pref = prefService.getDefaultBranch(null);
  var url = pref.getComplexValue("browser.startup.homepage",
                                  nsIPrefLocalizedString).data;
  var homePageField = document.getElementById("browserStartupHomepage");
  homePageField.value = url;
}

function Startup()
{  
  if (top.opener) {
    var browser = top.opener.document.getElementById("content");
    var l = browser.mPanelContainer.childNodes.length;

    if (l > 1) {
      var useButton = document.getElementById("browserUseCurrent");
      useButton.label = useButton.getAttribute("label2");
    }
  }
}
      
