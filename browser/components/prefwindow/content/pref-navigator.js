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
# The Original Code is the Firefox Options Dialog
#
# The Initial Developer of the Original Code is mozilla.org.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** -->

var _elementIDs = ["browserStartupHomepage", "checkForDefault"];
    
const nsIPrefService         = Components.interfaces.nsIPrefService;
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

function onOK() 
{
  if (!('homepage' in parent)) 
    return;

  // Replace pipes with commas to look nicer.
  parent.homepage = parent.homepage.replace(/\|/g,', ');
  
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
  var eb = windowManagerInterface.getEnumerator("navigator:browser");
  while (eb.hasMoreElements()) {
    // Update the home button tooltip.
    var domWin = eb.getNext().QueryInterface(Components.interfaces.nsIDOMWindow);
    var homeButton = domWin.document.getElementById("home-button");
    if (homeButton)
      homeButton.setAttribute("tooltiptext", parent.homepage);
  }
}

function Startup()
{
  var useButton = document.getElementById("browserUseCurrent");
  
  try {
    var browser = top.opener.document.getElementById("content");  

    var l = browser.mPanelContainer.childNodes.length;
    if (l > 1)
      useButton.label = useButton.getAttribute("label2");
  } catch (e) { 
    // prefwindow wasn't opened from a browser window, so no current page
    useButton.disabled = true;
  }
 
  try {
    var shellSvc = Components.classes["@mozilla.org/browser/shell-service;1"]
                             .getService(Components.interfaces.nsIShellService);
  } catch (e) {
    document.getElementById("defaultBrowserPrefs").hidden = true;
  }

  parent.hPrefWindow.registerOKCallbackFunc(onOK);
}
      
function showConnections()
{
  openDialog("chrome://browser/content/pref/pref-connection.xul", "", "centerscreen,chrome,modal=yes,dialog=yes");
}

function showFontsAndColors()
{
  openDialog("chrome://browser/content/pref/pref-fonts.xul", "", "centerscreen,chrome,modal=yes,dialog=yes");
}

function saveFontPrefs()
{
  var dataObject = top.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-fonts.xul"].userData;
  var pref = top.hPrefWindow.pref;
  for (var language in dataObject.languageData) {
    for (var type in dataObject.languageData[language].types) {
      var fontPrefString = "font.name." + type + "." + language;
      var currValue = "";
      try {
        currValue = pref.CopyUnicharPref(fontPrefString);
      }
      catch(e) {
      }
      if (currValue != dataObject.languageData[language].types[type])
        pref.SetUnicharPref(fontPrefString, dataObject.languageData[language].types[type]);
    }
    var variableSizePref = "font.size.variable." + language;
    var fixedSizePref = "font.size.fixed." + language;
    var minSizePref = "font.minimum-size." + language;
    var currVariableSize = 12, currFixedSize = 12, minSizeVal = 0;
    try {
      currVariableSize = pref.GetIntPref(variableSizePref);
      currFixedSize = pref.GetIntPref(fixedSizePref );
      minSizeVal = pref.GetIntPref(minSizePref);
    }
    catch(e) {
    }
    if (currVariableSize != dataObject.languageData[language].variableSize)
      pref.SetIntPref(variableSizePref, dataObject.languageData[language].variableSize);
    if (currFixedSize != dataObject.languageData[language].fixedSize)
      pref.SetIntPref(fixedSizePref, dataObject.languageData[language].fixedSize);
    if (minSizeVal != dataObject.languageData[language].minSize) {
      pref.SetIntPref(minSizePref, dataObject.languageData[language].minSize);
    }
  }

  // font scaling
  var fontDPI = parseInt(dataObject.fontDPI);
  var myFonts = dataObject.dataEls["useMyFonts"].checked;
  var defaultFont = dataObject.defaultFont;
  var myColors = dataObject.dataEls["useMyColors"].checked;
  try {
    var currDPI = pref.GetIntPref("browser.display.screen_resolution");
    var currFonts = pref.GetIntPref("browser.display.use_document_fonts");
    var currColors = pref.GetBoolPref("browser.display.use_document_colors");
    var currDefault = pref.CopyUnicharPref( "font.default" );
  }
  catch(e) {
  }

  if (currDPI != fontDPI)
    pref.SetIntPref("browser.display.screen_resolution", fontDPI);
  if (currFonts == myFonts)
    pref.SetIntPref("browser.display.use_document_fonts", !myFonts);
  if(currDefault != defaultFont)
    pref.SetUnicharPref( "font.default", defaultFont );
  if (currColors == myColors)
    pref.SetBoolPref("browser.display.use_document_colors", !myColors);

  var items = ["foregroundText", "background", "unvisitedLinks", "visitedLinks"];
  var prefs = ["browser.display.foreground_color", "browser.display.background_color",
                "browser.anchor_color", "browser.visited_color"];
  var prefvalue;
  for (var i = 0; i < items.length; ++i) {
    prefvalue = dataObject.dataEls[items[i]].value;
    pref.SetUnicharPref(prefs[i], prefvalue)
  }
  items = ["browserUseSystemColors", "browserUnderlineAnchors"];
  prefs = ["browser.display.use_system_colors", "browser.underline_anchors"];
  for (i = 0; i < items.length; ++i) {
    prefvalue = dataObject.dataEls[items[i]].checked;
    pref.SetBoolPref(prefs[i], prefvalue)
  }
}

# this function is only reached if the shell service is defined
function checkNow()
{
  var shellSvc = Components.classes["@mozilla.org/browser/shell-service;1"]
                           .getService(Components.interfaces.nsIShellService);
  var brandBundle = document.getElementById("bundle_brand");
  var shellBundle = document.getElementById("bundle_shell");
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

function showLanguages()
{
  openDialog("chrome://browser/content/pref/pref-languages.xul", "", "modal,centerscreen,resizable");
}
