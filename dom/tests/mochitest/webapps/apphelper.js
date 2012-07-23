/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const MODE_READONLY = 0x01;
const PERMS_FILE = 0644;

var popupNotifications = getPopupNotifications(window.top);

var event_listener_loaded = {};

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Webapps.jsm");

Components.classes["@mozilla.org/permissionmanager;1"]
          .getService(Components.interfaces.nsIPermissionManager)
          .addFromPrincipal(window.document.nodePrincipal,
                            "webapps-manage",
                             Components.interfaces.nsIPermissionManager.ALLOW_ACTION);

SpecialPowers.setCharPref("dom.mozApps.whitelist", "http://mochi.test:8888");
SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', true);
SpecialPowers.setBoolPref('browser.mozApps.installer.dry_run', true);
SpecialPowers.setBoolPref("dom.mozBrowserFramesWhitelist", "http://www.example.com");

let originalAAL = DOMApplicationRegistry.allAppsLaunchable;
DOMApplicationRegistry.allAppsLaunchable = true;

var triggered = false;

function iterateMethods(label, root, suppress) {
  var arr = [];
  for (var f in root) {
    if (suppress && suppress.indexOf(f) != -1)
      continue;
    if (typeof root[f] === 'function')
      arr.push(label + f);
   }
  return arr;
}

function getPopupNotifications(aWindow) {
  var Ci = Components.interfaces;
  var chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler.ownerDocument.defaultView;

  return chromeWin.PopupNotifications;
}

function triggerMainCommand(popup) {
  var notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  var notification = notifications[0];
  debug("triggering command: " + notification.getAttribute("buttonlabel"));

  notification.button.doCommand();
}

function mainCommand() {
  triggerMainCommand(this);
}

function popup_listener() {
  debug("here in popup listener"); 
  popupNotifications.panel.addEventListener("popupshown", mainCommand, false);
}

/**
 * Reads text from a file and returns the string.
 *
 * @param  aFile
 *         The file to read from.
 * @return The string of text read from the file.
 */
function readFile(aFile) {

  var file = Components.classes["@mozilla.org/file/directory_service;1"].
             getService(Components.interfaces.nsIProperties).
             get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis = Components.classes["@mozilla.org/network/file-input-stream;1"].
            createInstance(Components.interfaces.nsIFileInputStream);
  var paths = "chrome/dom/tests/mochitest/webapps" + aFile;
  var split = paths.split("/");
  var sis = Components.classes["@mozilla.org/scriptableinputstream;1"].
            createInstance(Components.interfaces.nsIScriptableInputStream);
  var utf8Converter = Components.classes["@mozilla.org/intl/utf8converterservice;1"].
    getService(Components.interfaces.nsIUTF8ConverterService);

  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, MODE_READONLY, PERMS_FILE, 0);
  sis.init(fis);
  var text = sis.read(sis.available());
  text = utf8Converter.convertURISpecToUTF8 (text, "UTF-8");
  sis.close();
  debug (text);
  return text;
}

function getOrigin(url) {
  return Services.io.newURI(url, null, null).prePath;
}

function tearDown() {
  debug("in " + arguments.callee.name);
  uninstallAll();
  popupNotifications.panel.removeEventListener("popupshown", mainCommand, false);
  SpecialPowers.clearUserPref('browser.mozApps.installer.dry_run');
  DOMApplicationRegistry.allAppsLaunchable = originalAAL;
}

