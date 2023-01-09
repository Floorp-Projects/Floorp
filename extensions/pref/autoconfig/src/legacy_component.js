//This config file is made by xiaoxiaoflood. Thanks. Pelase read about:license. 

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

if (Services.prefs.getBoolPref("toolkit.legacyUserProfileCustomizations.script", false)) {
  lockPref('xpinstall.signatures.required', false);
  lockPref('extensions.install_origins.enabled', false);

  const Constants = ChromeUtils.import('resource://gre/modules/AppConstants.jsm');
  const temp = Object.assign({}, Constants.AppConstants);
  temp.MOZ_REQUIRE_SIGNING = false
  Constants.AppConstants = Object.freeze(temp);
  
  try {
    let cmanifest = Cc['@mozilla.org/file/directory_service;1'].getService(Ci.nsIProperties).get('UChrm', Ci.nsIFile);
    cmanifest.append('utils');
    cmanifest.append('chrome.manifest');
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar).autoRegister(cmanifest);
  
    Cu.import('chrome://userchromejs/content/BootstrapLoader.jsm');
  } catch (ex) {};
  
  try {
    Cu.import('chrome://userchromejs/content/userChrome.jsm');
  } catch (ex) {};
} 
