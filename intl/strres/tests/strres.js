var strBundleService = null;
function srGetStrBundle(path, locale)
{
  var strBundle = null;

  strBundleService =
    Components.classes["component://netscape/intl/stringbundle"].createInstance(); 

  if (strBundleService) {
    strBundleService = 
      strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);

    if (strBundleService) {
      strBundle = strBundleService.CreateBundle(path, locale); 
      if (!strBundle) {
        dump("\n--** strbundle  createInstance failed **--\n");
		return null;
      }
    }
    else {
      dump("\n--** strBundleService createInstance 2 **--\n");
	  return null;
    }
  }
  else {
    dump("\n--** strBundleService createInstance 1 failed **--\n");
	return null;
  }
  return strBundle;
}
