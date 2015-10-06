var {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource:///modules/Services.jsm");
var dom_mozApps_debug = Services.prefs.getBoolPref("dom.mozApps.debug");
Services.prefs.setBoolPref("dom.mozApps.debug", true);
