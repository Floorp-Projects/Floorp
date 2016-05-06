/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

const kIMig = Ci.nsIBrowserProfileMigrator;
const kIPStartup = Ci.nsIProfileStartup;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");

var MigrationWizard = {
  _source: "",                  // Source Profile Migrator ContractID suffix
  _itemsFlags: kIMig.ALL,       // Selected Import Data Sources (16-bit bitfield)
  _selectedProfile: null,       // Selected Profile name to import from
  _wiz: null,
  _migrator: null,
  _autoMigrate: null,

  init: function ()
  {
    let os = Services.obs;
    os.addObserver(this, "Migration:Started", false);
    os.addObserver(this, "Migration:ItemBeforeMigrate", false);
    os.addObserver(this, "Migration:ItemAfterMigrate", false);
    os.addObserver(this, "Migration:ItemError", false);
    os.addObserver(this, "Migration:Ended", false);

    this._wiz = document.documentElement;

    let args = window.arguments;
    let entryPointId = args[0] || MigrationUtils.MIGRATION_ENTRYPOINT_UNKNOWN;
    Services.telemetry.getHistogramById("FX_MIGRATION_ENTRY_POINT").add(entryPointId);

    if (args.length > 1) {
      this._source = args[1];
      this._migrator = args[2] instanceof kIMig ?  args[2] : null;
      this._autoMigrate = args[3].QueryInterface(kIPStartup);
      this._skipImportSourcePage = args[4];
      if (this._migrator && args[5]) {
        let sourceProfiles = this._migrator.sourceProfiles;
        this._selectedProfile = sourceProfiles.find(profile => profile.id == args[5]);
      }

      if (this._autoMigrate) {
        // Show the "nothing" option in the automigrate case to provide an
        // easily identifiable way to avoid migration and create a new profile.
        document.getElementById("nothing").hidden = false;
      }
    }

    this.onImportSourcePageShow();
  },

  uninit: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "Migration:Started");
    os.removeObserver(this, "Migration:ItemBeforeMigrate");
    os.removeObserver(this, "Migration:ItemAfterMigrate");
    os.removeObserver(this, "Migration:ItemError");
    os.removeObserver(this, "Migration:Ended");
    MigrationUtils.finishMigration();
  },

  // 1 - Import Source
  onImportSourcePageShow: function ()
  {
    // Show warning message to close the selected browser when needed
    function toggleCloseBrowserWarning() {
      let visibility = "hidden";
      if (group.selectedItem.id != "nothing") {
        let migrator = MigrationUtils.getMigrator(group.selectedItem.id);
        visibility = migrator.sourceLocked ? "visible" : "hidden";
      }
      document.getElementById("closeSourceBrowser").style.visibility = visibility;
    }
    this._wiz.canRewind = false;

    var selectedMigrator = null;

    // Figure out what source apps are are available to import from:
    var group = document.getElementById("importSourceGroup");
    for (var i = 0; i < group.childNodes.length; ++i) {
      var migratorKey = group.childNodes[i].id;
      if (migratorKey != "nothing") {
        var migrator = MigrationUtils.getMigrator(migratorKey);
        if (migrator) {
          // Save this as the first selectable item, if we don't already have
          // one, or if it is the migrator that was passed to us.
          if (!selectedMigrator || this._source == migratorKey)
            selectedMigrator = group.childNodes[i];
        } else {
          // Hide this option
          group.childNodes[i].hidden = true;
        }
      }
    }

    group.addEventListener("command", toggleCloseBrowserWarning);

    if (selectedMigrator) {
      group.selectedItem = selectedMigrator;
      toggleCloseBrowserWarning();
    } else {
      // We didn't find a migrator, notify the user
      document.getElementById("noSources").hidden = false;

      this._wiz.canAdvance = false;

      document.getElementById("importBookmarks").hidden = true;
      document.getElementById("importAll").hidden = true;
    }

    // Advance to the next page if the caller told us to.
    if (this._migrator && this._skipImportSourcePage) {
      this._wiz.advance();
      this._wiz.canRewind = false;
    }
  },

  onImportSourcePageAdvanced: function ()
  {
    var newSource = document.getElementById("importSourceGroup").selectedItem.id;

    if (newSource == "nothing") {
      // Need to do telemetry here because we're closing the dialog before we get to
      // do actual migration. For actual migration, this doesn't happen until after
      // migration takes place.
      Services.telemetry.getHistogramById("FX_MIGRATION_SOURCE_BROWSER")
                        .add(MigrationUtils.getSourceIdForTelemetry("nothing"));
      document.documentElement.cancel();
      return false;
    }

    if (!this._migrator || (newSource != this._source)) {
      // Create the migrator for the selected source.
      this._migrator = MigrationUtils.getMigrator(newSource);

      this._itemsFlags = kIMig.ALL;
      this._selectedProfile = null;
    }
    this._source = newSource;

    // check for more than one source profile
    var sourceProfiles = this._migrator.sourceProfiles;
    if (this._skipImportSourcePage) {
      this._wiz.currentPage.next = "homePageImport";
    }
    else if (sourceProfiles && sourceProfiles.length > 1) {
      this._wiz.currentPage.next = "selectProfile";
    }
    else {
      if (this._autoMigrate)
        this._wiz.currentPage.next = "homePageImport";
      else
        this._wiz.currentPage.next = "importItems";

      if (sourceProfiles && sourceProfiles.length == 1)
        this._selectedProfile = sourceProfiles[0];
      else
        this._selectedProfile = null;
    }
    return undefined;
  },

  // 2 - [Profile Selection]
  onSelectProfilePageShow: function ()
  {
    // Disabling this for now, since we ask about import sources in automigration
    // too and don't want to disable the back button
    // if (this._autoMigrate)
    //   document.documentElement.getButton("back").disabled = true;

    var profiles = document.getElementById("profiles");
    while (profiles.hasChildNodes())
      profiles.removeChild(profiles.firstChild);

    // Note that this block is still reached even if the user chose 'From File'
    // and we canceled the dialog.  When that happens, _migrator will be null.
    if (this._migrator) {
      var sourceProfiles = this._migrator.sourceProfiles;

      for (let profile of sourceProfiles) {
        var item = document.createElement("radio");
        item.id = profile.id;
        item.setAttribute("label", profile.name);
        profiles.appendChild(item);
      }
    }

    profiles.selectedItem = this._selectedProfile ? document.getElementById(this._selectedProfile.id) : profiles.firstChild;
  },

  onSelectProfilePageRewound: function ()
  {
    var profiles = document.getElementById("profiles");
    this._selectedProfile = this._migrator.sourceProfiles.find(
      profile => profile.id == profiles.selectedItem.id
    ) || null;
  },

  onSelectProfilePageAdvanced: function ()
  {
    var profiles = document.getElementById("profiles");
    this._selectedProfile = this._migrator.sourceProfiles.find(
      profile => profile.id == profiles.selectedItem.id
    ) || null;

    // If we're automigrating or just doing bookmarks don't show the item selection page
    if (this._autoMigrate)
      this._wiz.currentPage.next = "homePageImport";
  },

  // 3 - ImportItems
  onImportItemsPageShow: function ()
  {
    var dataSources = document.getElementById("dataSources");
    while (dataSources.hasChildNodes())
      dataSources.removeChild(dataSources.firstChild);

    var items = this._migrator.getMigrateData(this._selectedProfile, this._autoMigrate);
    for (var i = 0; i < 16; ++i) {
      var itemID = (items >> i) & 0x1 ? Math.pow(2, i) : 0;
      if (itemID > 0) {
        var checkbox = document.createElement("checkbox");
        checkbox.id = itemID;
        checkbox.setAttribute("label",
          MigrationUtils.getLocalizedString(itemID + "_" + this._source));
        dataSources.appendChild(checkbox);
        if (!this._itemsFlags || this._itemsFlags & itemID)
          checkbox.checked = true;
      }
    }
  },

  onImportItemsPageRewound: function ()
  {
    this._wiz.canAdvance = true;
    this.onImportItemsPageAdvanced();
  },

  onImportItemsPageAdvanced: function ()
  {
    var dataSources = document.getElementById("dataSources");
    this._itemsFlags = 0;
    for (var i = 0; i < dataSources.childNodes.length; ++i) {
      var checkbox = dataSources.childNodes[i];
      if (checkbox.localName == "checkbox" && checkbox.checked)
        this._itemsFlags |= parseInt(checkbox.id);
    }
  },

  onImportItemCommand: function (aEvent)
  {
    var items = document.getElementById("dataSources");
    var checkboxes = items.getElementsByTagName("checkbox");

    var oneChecked = false;
    for (var i = 0; i < checkboxes.length; ++i) {
      if (checkboxes[i].checked) {
        oneChecked = true;
        break;
      }
    }

    this._wiz.canAdvance = oneChecked;
  },

  // 4 - Home Page Selection
  onHomePageMigrationPageShow: function ()
  {
    // only want this on the first run
    if (!this._autoMigrate) {
      this._wiz.advance();
      return;
    }

    var brandBundle = document.getElementById("brandBundle");
    // These strings don't exist when not using official branding. If that's
    // the case, just skip this page.
    try {
      var pageTitle = brandBundle.getString("homePageMigrationPageTitle");
      var pageDesc = brandBundle.getString("homePageMigrationDescription");
      var mainStr = brandBundle.getString("homePageSingleStartMain");
    }
    catch (e) {
      this._wiz.advance();
      return;
    }

    document.getElementById("homePageImport").setAttribute("label", pageTitle);
    document.getElementById("homePageImportDesc").setAttribute("value", pageDesc);

    this._wiz._adjustWizardHeader();

    var singleStart = document.getElementById("homePageSingleStart");
    singleStart.setAttribute("label", mainStr);
    singleStart.setAttribute("value", "DEFAULT");

    var source = null;
    switch (this._source) {
      case "ie":
        source = "sourceNameIE";
        break;
      case "safari":
        source = "sourceNameSafari";
        break;
      case "canary":
        source = "sourceNameCanary";
        break;
      case "chrome":
        source = "sourceNameChrome";
        break;
      case "chromium":
        source = "sourceNameChromium";
        break;
      case "firefox":
        source = "sourceNameFirefox";
        break;
      case "360se":
        source = "sourceName360se";
        break;
    }

    // semi-wallpaper for crash when multiple profiles exist, since we haven't initialized mSourceProfile in places
    this._migrator.getMigrateData(this._selectedProfile, this._autoMigrate);

    var oldHomePageURL = this._migrator.sourceHomePageURL;

    if (oldHomePageURL && source) {
      var appName = MigrationUtils.getLocalizedString(source);
      var oldHomePageLabel =
        brandBundle.getFormattedString("homePageImport", [appName]);
      var oldHomePage = document.getElementById("oldHomePage");
      oldHomePage.setAttribute("label", oldHomePageLabel);
      oldHomePage.setAttribute("value", oldHomePageURL);
      oldHomePage.removeAttribute("hidden");
    }
    else {
      // if we don't have at least two options, just advance
      this._wiz.advance();
    }
  },

  onHomePageMigrationPageAdvanced: function ()
  {
    // we might not have a selectedItem if we're in fallback mode
    try {
      var radioGroup = document.getElementById("homePageRadiogroup");

      this._newHomePage = radioGroup.selectedItem.value;
    } catch(ex) {}
  },

  // 5 - Migrating
  onMigratingPageShow: function ()
  {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.canRewind = false;
    this._wiz.canAdvance = false;

    // When automigrating, show all of the data that can be received from this source.
    if (this._autoMigrate)
      this._itemsFlags = this._migrator.getMigrateData(this._selectedProfile, this._autoMigrate);

    this._listItems("migratingItems");
    setTimeout(() => this.onMigratingMigrate(), 0);
  },

  onMigratingMigrate: function ()
  {
    this._migrator.migrate(this._itemsFlags, this._autoMigrate, this._selectedProfile);

    Services.telemetry.getHistogramById("FX_MIGRATION_SOURCE_BROWSER")
                      .add(MigrationUtils.getSourceIdForTelemetry(this._source));
    if (!this._autoMigrate) {
      let hist = Services.telemetry.getKeyedHistogramById("FX_MIGRATION_USAGE");
      let exp = 0;
      let items = this._itemsFlags;
      while (items) {
        if (items & 1) {
          hist.add(this._source, exp);
        }
        items = items >> 1;
        exp++
      }
    }
  },

  _listItems: function (aID)
  {
    var items = document.getElementById(aID);
    while (items.hasChildNodes())
      items.removeChild(items.firstChild);

    var brandBundle = document.getElementById("brandBundle");
    var itemID;
    for (var i = 0; i < 16; ++i) {
      itemID = (this._itemsFlags >> i) & 0x1 ? Math.pow(2, i) : 0;
      if (itemID > 0) {
        var label = document.createElement("label");
        label.id = itemID + "_migrated";
        try {
          label.setAttribute("value",
            MigrationUtils.getLocalizedString(itemID + "_" + this._source));
          items.appendChild(label);
        }
        catch (e) {
          // if the block above throws, we've enumerated all the import data types we
          // currently support and are now just wasting time, break.
          break;
        }
      }
    }
  },

  observe: function (aSubject, aTopic, aData)
  {
    var label;
    switch (aTopic) {
    case "Migration:Started":
      break;
    case "Migration:ItemBeforeMigrate":
      label = document.getElementById(aData + "_migrated");
      if (label)
        label.setAttribute("style", "font-weight: bold");
      break;
    case "Migration:ItemAfterMigrate":
      label = document.getElementById(aData + "_migrated");
      if (label)
        label.removeAttribute("style");
      break;
    case "Migration:Ended":
      if (this._autoMigrate) {
        Services.telemetry.getKeyedHistogramById("FX_MIGRATION_HOMEPAGE_IMPORTED")
                          .add(this._source, !!this._newHomePage);
        if (this._newHomePage) {
          try {
            // set homepage properly
            var prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                                    .getService(Components.interfaces.nsIPrefService);
            var prefBranch = prefSvc.getBranch(null);

            if (this._newHomePage == "DEFAULT") {
              prefBranch.clearUserPref("browser.startup.homepage");
            }
            else {
              var str = Components.classes["@mozilla.org/supports-string;1"]
                                .createInstance(Components.interfaces.nsISupportsString);
              str.data = this._newHomePage;
              prefBranch.setComplexValue("browser.startup.homepage",
                                         Components.interfaces.nsISupportsString,
                                         str);
            }

            var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                                   .getService(Components.interfaces.nsIProperties);
            var prefFile = dirSvc.get("ProfDS", Components.interfaces.nsIFile);
            prefFile.append("prefs.js");
            prefSvc.savePrefFile(prefFile);
          } catch(ex) {
            dump(ex);
          }
        }

        // We're done now.
        this._wiz.canAdvance = true;
        this._wiz.advance();

        setTimeout(close, 5000);
      }
      else {
        this._wiz.canAdvance = true;
        var nextButton = this._wiz.getButton("next");
        nextButton.click();
      }
      break;
    case "Migration:ItemError":
      let type = "undefined";
      let numericType = parseInt(aData);
      switch (numericType) {
      case Ci.nsIBrowserProfileMigrator.SETTINGS:
        type = "settings";
        break;
      case Ci.nsIBrowserProfileMigrator.COOKIES:
        type = "cookies";
        break;
      case Ci.nsIBrowserProfileMigrator.HISTORY:
        type = "history";
        break;
      case Ci.nsIBrowserProfileMigrator.FORMDATA:
        type = "form data";
        break;
      case Ci.nsIBrowserProfileMigrator.PASSWORDS:
        type = "passwords";
        break;
      case Ci.nsIBrowserProfileMigrator.BOOKMARKS:
        type = "bookmarks";
        break;
      case Ci.nsIBrowserProfileMigrator.OTHERDATA:
        type = "misc. data";
        break;
      }
      Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService)
        .logStringMessage("some " + type + " did not successfully migrate.");
      Services.telemetry.getKeyedHistogramById("FX_MIGRATION_ERRORS")
                        .add(this._source, Math.log2(numericType));
      break;
    }
  },

  onDonePageShow: function ()
  {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.canRewind = false;
    this._listItems("doneItems");
  }
};
