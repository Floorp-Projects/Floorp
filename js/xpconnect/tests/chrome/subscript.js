const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");

var ns = {};
Services.scriptloader.loadSubScript("resource://gre/modules/NetUtil.jsm", ns);
var NetUtil = ns.NetUtil;
