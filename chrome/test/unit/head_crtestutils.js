const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{4ba645d3-be6f-40d6-a42a-01b2f40091b8}");

let {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

function registerManifests(manifests) {
  var reg = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  for (var manifest of manifests)
    reg.autoRegister(manifest);
}
