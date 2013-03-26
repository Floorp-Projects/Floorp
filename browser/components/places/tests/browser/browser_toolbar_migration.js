/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests PersonalToolbar migration path.
 */

let bg = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);
let gOriginalMigrationVersion;
const BROWSER_URL = getBrowserURL();

let localStore = {
  get RDF() Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService),
  get store() this.RDF.GetDataSource("rdf:local-store"),

  get toolbar()
  {
    delete this.toolbar;
    let toolbar = this.RDF.GetResource(BROWSER_URL + "#PersonalToolbar");
    // Add the entry to the persisted set for this document if it's not there.
    // See XULDocument::Persist.
    let doc = this.RDF.GetResource(BROWSER_URL);
    let persist = this.RDF.GetResource("http://home.netscape.com/NC-rdf#persist");
    if (!this.store.HasAssertion(doc, persist, toolbar, true)) {
      this.store.Assert(doc, persist, toolbar, true);
    }
    return this.toolbar = toolbar;
  },

  getPersist: function getPersist(aProperty)
  {
    let property = this.RDF.GetResource(aProperty);
    let target = this.store.GetTarget(this.toolbar, property, true);
    if (target instanceof Ci.nsIRDFLiteral)
      return target.Value;
    return null;
  },

  setPersist: function setPersist(aProperty, aValue)
  {
    let property = this.RDF.GetResource(aProperty);
    let value = aValue ? this.RDF.GetLiteral(aValue) : null;

    try {
      let oldTarget = this.store.GetTarget(this.toolbar, property, true);
      if (oldTarget && value) {
        this.store.Change(this.toolbar, property, oldTarget, value);
      }
      else if (value) {
        this.store.Assert(this.toolbar, property, value, true);  
      }
      else if (oldTarget) {
        this.store.Unassert(this.toolbar, property, oldTarget);
      }
      else {
        return;
      }
    }
    catch(ex) {
      return;
    }
    this.store.QueryInterface(Ci.nsIRDFRemoteDataSource).Flush();
  }
};

let gTests = [

function test_explicitly_collapsed_toolbar()
{
  info("An explicitly collapsed toolbar should not be uncollapsed.");
  localStore.setPersist("collapsed", "true");
  bg.observe(null, "browser-glue-test", "force-ui-migration");
  is(localStore.getPersist("collapsed"), "true", "Toolbar is collapsed");
},

function test_customized_toolbar()
{
  info("A customized toolbar should be uncollapsed.");
  localStore.setPersist("currentset", "splitter");
  bg.observe(null, "browser-glue-test", "force-ui-migration");
  is(localStore.getPersist("collapsed"), "false", "Toolbar has been uncollapsed");
},

function test_many_bookmarks_toolbar()
{
  info("A toolbar with added bookmarks should be uncollapsed.");
  let ids = [];
  ids.push(
    PlacesUtils.bookmarks.insertSeparator(PlacesUtils.toolbarFolderId,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX)
  );
  ids.push(
    PlacesUtils.bookmarks.insertSeparator(PlacesUtils.toolbarFolderId,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX)
  );
  ids.push(
    PlacesUtils.bookmarks.insertSeparator(PlacesUtils.toolbarFolderId,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX)
  );
  bg.observe(null, "browser-glue-test", "force-ui-migration");
  is(localStore.getPersist("collapsed"), "false", "Toolbar has been uncollapsed");
},

];

function test()
{
  gOriginalMigrationVersion = Services.prefs.getIntPref("browser.migration.version");
  registerCleanupFunction(clean);

  if (localStore.getPersist("currentset") !== null) {
    info("Toolbar currentset was persisted by a previous test, fixing it.");
    localStore.setPersist("currentset", null);
  }

  if (localStore.getPersist("collapsed") !== null) {
    info("Toolbar collapsed status was persisted by a previous test, fixing it.");
    localStore.setPersist("collapsed", null);
  }

  while (gTests.length) {
    clean();
    Services.prefs.setIntPref("browser.migration.version", 4);
    gTests.shift().call();
  }
}

function clean()
{
  Services.prefs.setIntPref("browser.migration.version", gOriginalMigrationVersion);
  localStore.setPersist("currentset", null);
  localStore.setPersist("collapsed", null);
}

