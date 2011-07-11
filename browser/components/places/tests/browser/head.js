
// We need to cache this before test runs...
let cachedLeftPaneFolderIdGetter;
let (getter = PlacesUIUtils.__lookupGetter__("leftPaneFolderId")) {
  if (!cachedLeftPaneFolderIdGetter && typeof(getter) == "function")
    cachedLeftPaneFolderIdGetter = getter;
}
// ...And restore it when test ends.
registerCleanupFunction(function(){
  let (getter = PlacesUIUtils.__lookupGetter__("leftPaneFolderId")) {
    if (cachedLeftPaneFolderIdGetter && typeof(getter) != "function")
      PlacesUIUtils.__defineGetter__("leftPaneFolderId",
                                     cachedLeftPaneFolderIdGetter);
  }
});


function openLibrary(callback) {
  let library = window.openDialog("chrome://browser/content/places/places.xul",
                                  "", "chrome,toolbar=yes,dialog=no,resizable");
  waitForFocus(function () {
    callback(library);
  }, library);

  return library;
}

Components.utils.import("resource://gre/modules/NetUtil.jsm");
