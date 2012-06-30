# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

const UNKNOWN = nsIPermissionManager.UNKNOWN_ACTION;   // 0
const ALLOW = nsIPermissionManager.ALLOW_ACTION;       // 1
const BLOCK = nsIPermissionManager.DENY_ACTION;        // 2
const SESSION = nsICookiePermission.ACCESS_SESSION;    // 8

const nsIIndexedDatabaseManager =
  Components.interfaces.nsIIndexedDatabaseManager;

var gPermURI;
var gPrefs;

var gPermObj = {
  image: function getImageDefaultPermission()
  {
    if (gPrefs.getIntPref("permissions.default.image") == 2)
      return BLOCK;
    return ALLOW;
  },
  cookie: function getCookieDefaultPermission()
  {
    if (gPrefs.getIntPref("network.cookie.cookieBehavior") == 2)
      return BLOCK;

    if (gPrefs.getIntPref("network.cookie.lifetimePolicy") == 2)
      return SESSION;
    return ALLOW;
  },
  popup: function getPopupDefaultPermission()
  {
    if (gPrefs.getBoolPref("dom.disable_open_during_load"))
      return BLOCK;
    return ALLOW;
  },
  install: function getInstallDefaultPermission()
  {
    try {
      if (!gPrefs.getBoolPref("xpinstall.whitelist.required"))
        return ALLOW;
    }
    catch (e) {
    }
    return BLOCK;
  },
  geo: function getGeoDefaultPermissions()
  {
    return BLOCK;
  },
  indexedDB: function getIndexedDBDefaultPermissions()
  {
    return UNKNOWN;
  },
  plugins: function getPluginsDefaultPermissions()
  {
    if (gPrefs.getBoolPref("plugins.click_to_play"))
      return BLOCK;
    return ALLOW;
  },
  fullscreen: function getFullscreenDefaultPermissions()
  {
    return UNKNOWN;  
  }
};

var permissionObserver = {
  observe: function (aSubject, aTopic, aData)
  {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Components.interfaces.nsIPermission);
      if (permission.host == gPermURI.host && permission.type in gPermObj)
        initRow(permission.type);
    }
  }
};

function onLoadPermission()
{
  gPrefs = Components.classes[PREFERENCES_CONTRACTID]
                     .getService(Components.interfaces.nsIPrefBranch);

  var uri = gDocument.documentURIObject;
  var permTab = document.getElementById("permTab");
  if(/^https?/.test(uri.scheme)) {
    gPermURI = uri;
    var hostText = document.getElementById("hostText");
    hostText.value = gPermURI.host;

    for (var i in gPermObj)
      initRow(i);
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(permissionObserver, "perm-changed", false);
    onUnloadRegistry.push(onUnloadPermission);
    permTab.hidden = false;
  }
  else
    permTab.hidden = true;
}

function onUnloadPermission()
{
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.removeObserver(permissionObserver, "perm-changed");

  var dbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"]
                            .getService(nsIIndexedDatabaseManager);
  dbManager.cancelGetUsageForURI(gPermURI, onIndexedDBUsageCallback);
}

function initRow(aPartId)
{
  if (aPartId == "plugins" && !gPrefs.getBoolPref("plugins.click_to_play"))
    document.getElementById("permPluginsRow").hidden = true;

  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);

  var checkbox = document.getElementById(aPartId + "Def");
  var command  = document.getElementById("cmd_" + aPartId + "Toggle");
  // Geolocation permission consumers use testExactPermission, not testPermission. 
  var perm = aPartId == "geo" ? permissionManager.testExactPermission(gPermURI, aPartId) :
                                permissionManager.testPermission(gPermURI, aPartId);
  if (perm) {
    checkbox.checked = false;
    command.removeAttribute("disabled");
  }
  else {
    checkbox.checked = true;
    command.setAttribute("disabled", "true");
    perm = gPermObj[aPartId]();
  }
  setRadioState(aPartId, perm);

  if (aPartId == "indexedDB") {
    initIndexedDBRow();
  }
}

function onCheckboxClick(aPartId)
{
  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);

  var command  = document.getElementById("cmd_" + aPartId + "Toggle");
  var checkbox = document.getElementById(aPartId + "Def");
  if (checkbox.checked) {
    permissionManager.remove(gPermURI.host, aPartId);
    command.setAttribute("disabled", "true");
    var perm = gPermObj[aPartId]();
    setRadioState(aPartId, perm);
  }
  else {
    onRadioClick(aPartId);
    command.removeAttribute("disabled");
  }
}

function onRadioClick(aPartId)
{
  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);

  var radioGroup = document.getElementById(aPartId + "RadioGroup");
  var id = radioGroup.selectedItem.id;
  var permission = id.split('#')[1];
  permissionManager.add(gPermURI, aPartId, permission);
  if (aPartId == "indexedDB" &&
      (permission == ALLOW || permission == BLOCK)) {
    permissionManager.remove(gPermURI.host, "indexedDB-unlimited");
  }
  if (aPartId == "fullscreen" && permission == UNKNOWN) {
    permissionManager.remove(gPermURI.host, "fullscreen");
  }  
}

function setRadioState(aPartId, aValue)
{
  var radio = document.getElementById(aPartId + "#" + aValue);
  radio.radioGroup.selectedItem = radio;
}

function initIndexedDBRow()
{
  var dbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"]
                            .getService(nsIIndexedDatabaseManager);
  dbManager.getUsageForURI(gPermURI, onIndexedDBUsageCallback);

  var status = document.getElementById("indexedDBStatus");
  var button = document.getElementById("indexedDBClear");

  status.value = "";
  status.setAttribute("hidden", "true");
  button.setAttribute("hidden", "true");
}

function onIndexedDBClear()
{
  Components.classes["@mozilla.org/dom/indexeddb/manager;1"]
            .getService(nsIIndexedDatabaseManager)
            .clearDatabasesForURI(gPermURI);

  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);
  permissionManager.remove(gPermURI.host, "indexedDB-unlimited");
  initIndexedDBRow();
}

function onIndexedDBUsageCallback(uri, usage, fileUsage)
{
  if (!uri.equals(gPermURI)) {
    throw new Error("Callback received for bad URI: " + uri);
  }

  if (usage) {
    if (!("DownloadUtils" in window)) {
      Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
    }

    var status = document.getElementById("indexedDBStatus");
    var button = document.getElementById("indexedDBClear");

    status.value =
      gBundle.getFormattedString("indexedDBUsage",
                                 DownloadUtils.convertByteUnits(usage));
    status.removeAttribute("hidden");
    button.removeAttribute("hidden");
  }
}
