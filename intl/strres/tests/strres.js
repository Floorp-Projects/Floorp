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
