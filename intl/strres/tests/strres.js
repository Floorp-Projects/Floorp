var strBundleService = null;

function srGetAppLocale()
{
  var localeService = null;
  var applicationLocale = null;

  localeService = Components.classes["component://netscape/intl/nslocaleservice"].createInstance();
  if (!localeService) {
    dump("\n--** localeService createInstance 1 failed **--\n");
	return null;
  }

  localeService = localeService.QueryInterface(Components.interfaces.nsILocaleService);
  if (!localeService) {
    dump("\n--** localeService createInstance 2 failed **--\n");
	return null;
  }
  applicationLocale = localeService.GetApplicationLocale();
  if (!applicationLocale) {
    dump("\n--** localeService.GetApplicationLocale failed **--\n");
  }
  return applicationLocale;
}

function srGetStrBundleWithLocale(path, locale)
{
  var strBundle = null;

  strBundleService =
    Components.classes["component://netscape/intl/stringbundle"].createInstance(); 

  if (!strBundleService) {
    dump("\n--** strBundleService createInstance 1 failed **--\n");
	return null;
  }

  strBundleService = 
  	strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);

  if (!strBundleService) {
	dump("\n--** strBundleService createInstance 2 failed **--\n");
	return null;
  }	

  strBundle = strBundleService.CreateBundle(path, locale); 
  if (!strBundle) {
	dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

function srGetStrBundle(path)
{
  var appLocale = srGetAppLocale();
  return srGetStrBundleWithLocale(path, appLocale);
}


function localeSwitching(baseDirectory, providerName)
{
  dump("\n ** Enter localeSwitching() ** \n");
  dump("\n ** baseDirectory=" +  baseDirectory + " ** \n");
  dump("\n ** providerName=" +  providerName + " ** \n");

  //
  var rdf;
  if(document.rdf) {
    rdf = document.rdf;
    dump("\n ** rdf = document.rdf ** \n");
  }
  else if(Components) {
    var isupports = Components.classes['component://netscape/rdf/rdf-service'].getService();
    rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
    dump("\n ** rdf = Components... ** \n");
  }
  else {
    dump("can't find nuthin: no document.rdf, no Components. \n");
  }
  //

  var ds = rdf.GetDataSource("rdf:chrome");

  // For M4 builds, use this line instead.
  // var ds = rdf.GetDataSource("resource:/chrome/registry.rdf");

  var sourceNode = rdf.GetResource("chrome://navigator/locale/");
  var baseArc = rdf.GetResource("http://chrome.mozilla.org/rdf#base");
  var nameArc = rdf.GetResource("http://chrome.mozilla.org/rdf#name");
                      
  // Get the old targets
  var oldBaseTarget = ds.GetTarget(sourceNode, baseArc, true);
  dump("\n** oldBaseTarget=" + oldBaseTarget + "**\n");
  var oldNameTarget = ds.GetTarget(sourceNode, nameArc, true);
  dump("\n** oldNameTarget=" + oldNameTarget + "**\n");

  // Get the new targets 
  // file:/u/tao/gila/mozilla-org/html/projects/intl/chrome/
  // da-DK
  var newBaseTarget = rdf.GetLiteral(baseDirectory);
  var newNameTarget = rdf.GetLiteral(providerName);
  
  // Unassert the old relationships
  //ds.Unassert(sourceNode, baseArc, oldBaseTarget);
  ds.Unassert(sourceNode, nameArc, oldNameTarget);
  
  // Assert the new relationships (note that we want a reassert rather than
  // an unassert followed by an assert, once reassert is implemented)
  //ds.Assert(sourceNode, baseArc, newBaseTarget, true);
  ds.Assert(sourceNode, nameArc, newNameTarget, true);
  
  // Flush the modified data source to disk
  // (Note: crashes in M4 builds, so don't use Flush() until fix checked in)
  // ds.Flush();

  // Open up a new window to see your new chrome, since changes aren't yet dynamically
  // applied to the current window

  BrowserOpenWindow('chrome://navigator/content');
  dump("\n ** Leave localeSwitching() ** \n");
}

function localeTo(localeName)
{
  dump("\n ** Enter localeTo() ** \n");
  localeSwitching("",
               	  localeName);
  dump("\n ** Leave localeTo() ** \n");
}
