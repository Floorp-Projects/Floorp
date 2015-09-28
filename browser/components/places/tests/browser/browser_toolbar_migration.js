/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests PersonalToolbar migration path.
 */
var bg = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);
var gOriginalMigrationVersion;
const BROWSER_URL = getBrowserURL();

var localStore = {
  get xulStore() {
    return Cc["@mozilla.org/xul/xulstore;1"].getService(Ci.nsIXULStore);
  },

  getValue: function getValue(aProperty)
  {
    return this.xulStore.getValue(BROWSER_URL, "PersonalToolbar", aProperty);
  },

  setValue: function setValue(aProperty, aValue)
  {
    if (aValue) {
      this.xulStore.setValue(BROWSER_URL, "PersonalToolbar", aProperty, aValue);
    } else {
      this.xulStore.removeValue(BROWSER_URL, "PersonalToolbar", aProperty);
    }
  }
};

var gTests = [

function test_explicitly_collapsed_toolbar()
{
  info("An explicitly collapsed toolbar should not be uncollapsed.");
  localStore.setValue("collapsed", "true");
  bg.observe(null, "browser-glue-test", "force-ui-migration");
  is(localStore.getValue("collapsed"), "true", "Toolbar is collapsed");
},

function test_customized_toolbar()
{
  info("A customized toolbar should be uncollapsed.");
  localStore.setValue("currentset", "splitter");
  bg.observe(null, "browser-glue-test", "force-ui-migration");
  is(localStore.getValue("collapsed"), "false", "Toolbar has been uncollapsed");
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
  ids.push(
    PlacesUtils.bookmarks.insertSeparator(PlacesUtils.toolbarFolderId,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX)
  );
  bg.observe(null, "browser-glue-test", "force-ui-migration");
  is(localStore.getValue("collapsed"), "false", "Toolbar has been uncollapsed");
},

];

function test()
{
  gOriginalMigrationVersion = Services.prefs.getIntPref("browser.migration.version");
  registerCleanupFunction(clean);

  if (localStore.getValue("currentset") !== null) {
    info("Toolbar currentset was persisted by a previous test, fixing it.");
    localStore.setValue("currentset", null);
  }

  if (localStore.getValue("collapsed") !== null) {
    info("Toolbar collapsed status was persisted by a previous test, fixing it.");
    localStore.setValue("collapsed", null);
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
  localStore.setValue("currentset", null);
  localStore.setValue("collapsed", null);
}

