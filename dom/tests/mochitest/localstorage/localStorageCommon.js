function localStorageFlush(cb)
{
  var ob = {
    observe : function(sub, top, dat)
    {
      os().removeObserver(ob, "domstorage-test-flushed");
      cb();
    }
  };
  os().addObserver(ob, "domstorage-test-flushed", false);
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
  return SpecialPowers.Cc["@mozilla.org/observer-service;1"]
                      .getService(SpecialPowers.Ci.nsIObserverService);
}

function notify(top)
{
  os().notifyObservers(null, top, null);
}
