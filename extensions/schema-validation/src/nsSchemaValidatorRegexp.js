const kSERVICE_CONTRACTID = "@mozilla.org/xmlextras/schemas/schemavalidatorregexp;1";
const kSERVICE_CID = Components.ID("26d69f7e-f7cf-423d-afb9-43d8a9ebf3ba");
const nsISupports = Components.interfaces.nsISupports;

function nsSchemaValidatorRegexp()
{
}

nsSchemaValidatorRegexp.prototype.runRegexp = function(aString, aRegexpString, aModifierString) {
  var rv = false;

//  dump("\n  String   is: " + aString);
//  dump("\n  Regexp   is: " + aRegexpString);
//  dump("\n  Modifier is: " + aModifierString);

  var myRegExp = RegExp(aRegexpString, aModifierString);

  if (aString.match(myRegExp)) {
    rv = true;
  }

  return rv;
}

nsSchemaValidatorRegexp.prototype.QueryInterface = function(iid) {
  if (!iid.equals(Components.interfaces.nsISchemaValidatorRegexp) &&
      !iid.equals(Components.interfaces.nsISchemaValidator) &&
      !iid.equals(nsISupports))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  return this;
}


/**
 * JS XPCOM component registration goop:
 *
 * We set ourselves up to observe the xpcom-startup category.  This provides
 * us with a starting point.
 */

var nsSchemaValidatorRegexpModule = new Object();

nsSchemaValidatorRegexpModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(kSERVICE_CID,
                                  "nsSchemaValidatorRegexp",
                                  kSERVICE_CONTRACTID,
                                  fileSpec, 
                                  location, 
                                  type);
}

nsSchemaValidatorRegexpModule.getClassObject =
function (compMgr, cid, iid)
{
  if (!cid.equals(kSERVICE_CID))
    throw Components.results.NS_ERROR_NO_INTERFACE;

  if (!iid.equals(Components.interfaces.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return nsSchemaValidatorRegexpFactory;
}

nsSchemaValidatorRegexpModule.canUnload =
function (compMgr)
{
  return true;
}

var nsSchemaValidatorRegexpFactory = new Object();

nsSchemaValidatorRegexpFactory.createInstance =
function (outer, iid)
{
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;

  if (!iid.equals(Components.interfaces.nsISchemaValidatorRegexp) &&
      !iid.equals(nsISupports))
    throw Components.results.NS_ERROR_NO_INTERFACE;

  return new nsSchemaValidatorRegexp();
}

function NSGetModule(compMgr, fileSpec)
{
  return nsSchemaValidatorRegexpModule;
}
