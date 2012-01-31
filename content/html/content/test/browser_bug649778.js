// Test for bug 649778 - document.write may cause a document to be written to disk cache even when the page has Cache-Control: no-store

// Globals
var testPath = "http://mochi.test:8888/browser/content/html/content/test/";
var popup;

function checkCache(url, policy, shouldExist)
{
  var cache = Components.classes["@mozilla.org/network/cache-service;1"].
              getService(Components.interfaces.nsICacheService);
  var session = cache.createSession(
                  "wyciwyg", policy,
                  Components.interfaces.nsICache.STREAM_BASED);
  try {
    var cacheEntry = session.openCacheEntry(
                       url, Components.interfaces.nsICache.ACCESS_READ, true);
    is(shouldExist, true, "Entry found");
  }
  catch (e) {
    is(shouldExist, false, "Entry not found");
    is(e.result, Components.results.NS_ERROR_CACHE_KEY_NOT_FOUND,
       "Invalid error");
  }
}

function getPopupURL() {
  var sh = popup.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIWebNavigation)
                .sessionHistory;

  return sh.getEntryAtIndex(sh.index, false).URI.spec;
}

function testContinue() {
  var wyciwygURL = getPopupURL();
  is(wyciwygURL.substring(0, 10), "wyciwyg://", "Unexpected URL.");
  popup.close()

  checkCache(wyciwygURL, Components.interfaces.nsICache.STORE_ON_DISK, false);
  checkCache(wyciwygURL, Components.interfaces.nsICache.STORE_IN_MEMORY, true);

  finish();
}

function waitForWyciwygDocument() {
  try {
    var url = getPopupURL();
    if (url.substring(0, 10) == "wyciwyg://") {
      setTimeout(testContinue, 0);
      return;
    }
  }
  catch (e) {
  }
  setTimeout(waitForWyciwygDocument, 100);
}

// Entry point from Mochikit
function test() {
  waitForExplicitFinish();

  popup = window.open(testPath + "file_bug649778.html", "popup 0",
                      "height=200,width=200,location=yes," +
                      "menubar=yes,status=yes,toolbar=yes,dependent=yes");

  waitForWyciwygDocument();
}
