// Test for bug 649778 - document.write may cause a document to be written to disk cache even when the page has Cache-Control: no-store

// Globals
var testPath = "http://mochi.test:8888/browser/content/html/content/test/";
var popup;

function checkCache(url, policy, shouldExist, cb)
{
  var cache = Components.classes["@mozilla.org/network/cache-service;1"].
              getService(Components.interfaces.nsICacheService);
  var session = cache.createSession(
                  "wyciwyg", policy,
                  Components.interfaces.nsICache.STREAM_BASED);

  var checkCacheListener = {
    onCacheEntryAvailable: function oCEA(entry, access, status) {
      if (shouldExist) {
        ok(entry, "Entry not found");
        is(status, Components.results.NS_OK, "Entry not found");
      } else {
        ok(!entry, "Entry found");
        is(status, Components.results.NS_ERROR_CACHE_KEY_NOT_FOUND,
           "Invalid error code");
      }

      setTimeout(cb, 0);
    }
  };

  session.asyncOpenCacheEntry(url,
                              Components.interfaces.nsICache.ACCESS_READ,
                              checkCacheListener);
}
function getPopupURL() {
  var sh = popup.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIWebNavigation)
                .sessionHistory;

  return sh.getEntryAtIndex(sh.index, false).URI.spec;
}

var wyciwygURL;
function testContinue() {
  wyciwygURL = getPopupURL();
  is(wyciwygURL.substring(0, 10), "wyciwyg://", "Unexpected URL.");
  popup.close()

  checkCache(wyciwygURL, Components.interfaces.nsICache.STORE_ON_DISK, false,
    function() {
      checkCache(wyciwygURL, Components.interfaces.nsICache.STORE_IN_MEMORY,
                 true, finish);
    });
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
