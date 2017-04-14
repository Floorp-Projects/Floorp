function localStorageFlush(cb)
{
  var ob = {
    observe : function(sub, top, dat)
    {
      os().removeObserver(ob, "domstorage-test-flushed");
      cb();
    }
  };
  os().addObserver(ob, "domstorage-test-flushed");
  notify("domstorage-test-flush-force");
}

function localStorageReload()
{
  notify("domstorage-test-reload");
}

function localStorageFlushAndReload(cb)
{
  localStorageFlush(function() {
    localStorageReload();
    cb();
  });
}

function localStorageClearAll()
{
  os().notifyObservers(null, "cookie-changed", "cleared");
}

function localStorageClearDomain(domain)
{
  os().notifyObservers(null, "browser:purge-domain-data", domain);
}

function os()
{
  return SpecialPowers.Services.obs;
}

function notify(top)
{
  os().notifyObservers(null, top, null);
}

/**
 * Enable testing observer notifications in DOMStorageObserver.cpp.
 */
function localStorageEnableTestingMode(cb)
{
  SpecialPowers.pushPrefEnv({ "set": [["dom.storage.testing", true]] }, cb);
}
