const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{4ba645d3-be6f-40d6-a42a-01b2f40091b8}");

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;


function registerManifests(manifests)
{
  var reg = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  for each (var manifest in manifests)
    reg.autoRegister(manifest);
}
