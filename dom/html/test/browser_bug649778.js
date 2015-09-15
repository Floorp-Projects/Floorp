// Test for bug 649778 - document.write may cause a document to be written to disk cache even when the page has Cache-Control: no-store

// Globals
var testPath = "http://mochi.test:8888/browser/dom/html/test/";
var popup;

var {LoadContextInfo} = Cu.import("resource://gre/modules/LoadContextInfo.jsm", null);
var {Services} = Cu.import("resource://gre/modules/Services.jsm", null);

function checkCache(url, inMemory, shouldExist, cb)
{
  var cache = Services.cache2;
  var storage = cache.diskCacheStorage(LoadContextInfo.default, false);

  function CheckCacheListener(inMemory, shouldExist)
  {
    this.inMemory = inMemory;
    this.shouldExist = shouldExist;
    this.onCacheEntryCheck = function() {
      return Components.interfaces.nsICacheEntryOpenCallback.ENTRY_WANTED;
    };

    this.onCacheEntryAvailable = function oCEA(entry, isNew, appCache, status) {
      if (shouldExist) {
        ok(entry, "Entry not found");
        is(this.inMemory, !entry.persistent, "Entry is " + (inMemory ? "" : " not ") + " in memory as expected");
        is(status, Components.results.NS_OK, "Entry not found");
      } else {
        ok(!entry, "Entry found");
        is(status, Components.results.NS_ERROR_CACHE_KEY_NOT_FOUND,
           "Invalid error code");
      }

      setTimeout(cb, 0);
    };
  };

  storage.asyncOpenURI(Services.io.newURI(url, null, null), "",
                       Components.interfaces.nsICacheStorage.OPEN_READONLY,
                       new CheckCacheListener(inMemory, shouldExist));
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

  // We have to find the entry and it must not be persisted to disk
  checkCache(wyciwygURL, true, true, finish);
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
