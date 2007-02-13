/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Calendar migration code
 *
 * The Initial Developer of the Original Code is
 *   Joey Minta <jminta@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matthew Willis <mattwillis@gmail.com>
 *   Clint Talbert <cmtalbert@myfastmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const SUNBIRD_UID = "{718e30fb-e89b-41dd-9da7-e25a45638b28}";
const FIREFOX_UID = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

//
// The front-end wizard bits.
//
var gMigrateWizard = {
    /**
     * Called from onload of the migrator window.  Takes all of the migrators
     * that were passed in via window.arguments and adds them to checklist. The
     * user can then check these off to migrate the data from those sources.
     */
    loadMigrators: function gmw_load() {
        var listbox = document.getElementById("datasource-list");

        //XXX Once we have branding for lightning, this hack can go away
        var sbs = Cc["@mozilla.org/intl/stringbundle;1"]
                  .getService(Ci.nsIStringBundleService);

        var props = sbs.createBundle("chrome://calendar/locale/migration.properties");

        if (gDataMigrator.isLightning()) {
            var wizard = document.getElementById("migration-wizard");
            var desc = document.getElementById("wizard-desc");
            // Since we don't translate "Lightning"...
            wizard.title = props.formatStringFromName("migrationTitle",
                                                      ["Lightning"],
                                                      1);
            desc.textContent = props.formatStringFromName("migrationDescription",
                                                          ["Lightning"],
                                                          1);
        }

        LOG("migrators: " + window.arguments.length);
        for each (var migrator in window.arguments[0]) {
            var listItem = document.createElement("listitem");
            listItem.setAttribute("type", "checkbox");
            listItem.setAttribute("checked", true);
            listItem.setAttribute("label", migrator.title);
            listItem.migrator = migrator;
            listbox.appendChild(listItem);
        }
    },

    /**
     * Called from the second page of the wizard.  Finds all of the migrators
     * that were checked and begins migrating their data.  Also controls the
     * progress dialog so the user can see what is happening. (somewhat)
     */
    migrateChecked: function gmw_migrate() {
        var migrators = [];

        // Get all the checked migrators into an array
        var listbox = document.getElementById("datasource-list");
        for (var i = listbox.childNodes.length-1; i >= 0; i--) {
            LOG("Checking child node: " + listbox.childNodes[i]);
            if (listbox.childNodes[i].getAttribute("checked")) {
                LOG("Adding migrator");
                migrators.push(listbox.childNodes[i].migrator);
            }
        }

        // If no migrators were checked, then we're done
        if (migrators.length == 0) {
            window.close();
        }

        // Don't let the user get away while we're migrating
        //XXX may want to wire this into the 'cancel' function once that's
        //    written
        var wizard = document.getElementById("migration-wizard");
        wizard.canAdvance = false;
        wizard.canRewind = false;

        // We're going to need this for the progress meter's description
        var sbs = Cc["@mozilla.org/intl/stringbundle;1"]
                  .getService(Ci.nsIStringBundleService);
        var props = sbs.createBundle("chrome://calendar/locale/migration.properties");
        var label = document.getElementById("progress-label");
        var meter = document.getElementById("migrate-progressmeter");

        var i = 0;
        // Because some of our migrators involve async code, we need this
        // call-back function so we know when to start the next migrator.
        function getNextMigrator() {
            if (migrators[i]) {
                var mig = migrators[i];

                // Increment i to point to the next migrator
                i++;
                LOG("starting migrator: " + mig.title);
                label.value = props.formatStringFromName("migratingApp",
                                                         [mig.title],
                                                         1);
                meter.value = (i-1)/migrators.length*100;
                mig.args.push(getNextMigrator);

                try {
                    mig.migrate.apply(mig, mig.args);
                } catch (e) {
                    LOG("Failed to migrate: " + mig.title);
                    LOG(e);
                    getNextMigrator();
                }
            } else {
                LOG("migration done");
                wizard.canAdvance = true;
                label.value = props.GetStringFromName("finished");
                meter.value = 100;
                gMigrateWizard.setCanRewindFalse();
            }
         }

        // And get the first migrator
        getNextMigrator();
   },

    setCanRewindFalse: function gmw_finish() {
        document.getElementById('migration-wizard').canRewind = false;
    }
};

//
// The more back-end data detection bits
//
function dataMigrator(aTitle, aMigrateFunction, aArguments) {
    this.title = aTitle;
    this.migrate = aMigrateFunction;
    this.args = aArguments;
}

var gDataMigrator = {
    mIsLightning: null,
    mIsInFirefox: false,
    mPlatform: null,
    mDirService: null,
    mIoService: null,

    /**
     * Properly caches the service so that it doesn't load on startup
     */
    get dirService() {
        if (!this.mDirService) {
            this.mDirService = Cc["@mozilla.org/file/directory_service;1"]
                               .getService(Ci.nsIProperties);
        }
        return this.mDirService;
    },
    
    get ioService() {
        if (!this.mIoService) {
            this.mIoService = Cc["@mozilla.org/network/io-service;1"]
                              .getService(Ci.nsIIOService);
        }
        return this.mIoService;
    },

    /**
     * Gets the value for mIsLightning, and sets it if this.mIsLightning is
     * not initialized. This is used by objects outside gDataMigrator to
     * access the mIsLightning member.
     */
    isLightning: function is_ltn() {
        if (this.mIsLightning == null) {
            var appInfo = Cc["@mozilla.org/xre/app-info;1"]
                          .getService(Ci.nsIXULAppInfo);
            this.mIsLightning = !(appInfo.ID == SUNBIRD_UID);
            return this.mIsLightning;
        }
        // else mIsLightning is initialized, return the value
        return this.mIsLightning;
    },

    /**
     * Call to do a general data migration (for a clean profile)  Will run
     * through all of the known migrator-checkers.  These checkers will return
     * an array of valid dataMigrator objects, for each kind of data they find.
     * If there is at least one valid migrator, we'll pop open the migration
     * wizard, otherwise, we'll return silently.
     */
    checkAndMigrate: function gdm_migrate() {
        var appInfo = Cc["@mozilla.org/xre/app-info;1"]
                      .getService(Ci.nsIXULAppInfo);
        this.mIsLightning = !(appInfo.ID == SUNBIRD_UID)
        LOG("mIsLightning is: " + this.mIsLightning);
        if (appInfo.ID == FIREFOX_UID) {
            this.mIsInFirefox = true;
            // We can't handle Firefox Lightning yet
            LOG("Holy cow, you're Firefox-Lightning! sorry, can't help.");
            return;
        }

        var xulRuntime = Cc["@mozilla.org/xre/app-info;1"]
                         .getService(Ci.nsIXULRuntime);
        this.mPlatform = xulRuntime.OS.toLowerCase();

        LOG("mPlatform is: " + this.mPlatform);

        var DMs = [];
        var migrators = [this.checkOldCal, this.checkEvolution,
                         this.checkIcal];
        // XXX also define a category and an interface here for pluggability
        for each (var migrator in migrators) {
            var migs = migrator.call(this);
            for each (var dm in migs) {
                DMs.push(dm);
            }
        }

        if (DMs.length == 0) {
            // No migration available
            return;
        }
        LOG("DMs: " + DMs.length);

        var url = "chrome://calendar/content/migration.xul";
#ifdef XP_MACOSX
        var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                           .getService(Components.interfaces.nsIWindowMediator);
        var win = wm.getMostRecentWindow("Calendar:MigrationWizard");
        if (win) {
            win.focus();
        } else {
            openDialog(url, "migration", "centerscreen,chrome,resizable=no", DMs);
        }
#else
        openDialog(url, "migration", "modal,centerscreen,chrome,resizable=no", DMs);
#endif
    },

    /**
     * Checks to see if we can find any traces of an older moz-cal program. 
     * This could be either the old calendar-extension, or Sunbird 0.2.  If so, 
     * it offers to move that data into our new storage format.  Also, if we're
     * if we're Lightning, it will disable the old calendar extension, since it
     * conflicts with us.
     */
    checkOldCal: function gdm_calold() {
        LOG("Checking for the old calendar extension/app");

        // First things first.  If we are Lightning and the calendar extension
        // is installed, we have to nuke it.  The old extension defines some of
        // the same paths as we do, and the resulting file conflicts result in
        // first-class badness. getCompositeCalendar is a conflicting function
        // that exists in Lighnting's version of calUtils.js.  If it isn't
        // defined, we have a conflict.
        if (this.isLightning() && !("getCompositeCalendar" in window)) {

            // We can't use our normal helper-functions, because those might
            // conflict too.
            var sbs = Cc["@mozilla.org/intl/stringbundle;1"]
                      .getService(Ci.nsIStringBundleService);
            var props = sbs.createBundle("chrome://calendar/locale/migration.properties");
            var brand = sbs.createBundle("chrome://branding/locale/brand.properties");
            var appName = brand.GetStringFromName("brandShortName");
            // Tell the user we're going to disable and restart
            var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Ci.nsIPromptService); 
            promptService.alert(window,
                                props.GetStringFromName("disableExtTitle"),
                                props.formatStringFromName("disableExtText",
                                                           [brand],1));

            // Kiiillllll...
            var em = Cc["@mozilla.org/extensions/manager;1"]
                     .getService(Ci.nsIExtensionManager);
            em.disableItem("{8e117890-a33f-424b-a2ea-deb272731365}");
            promptService.alert(window, getString("disableDoneTitle"),
                                getString("disableExtDone"));
            var startup = Cc["@mozilla.org/toolkit/app-startup;1"]
                          .getService(Ci.nsIAppStartup);
            startup.quit(Ci.nsIAppStartup.eRestart | 
                         Ci.nsIAppStartup.eAttemptQuit);
        }

        // This is the function that the migration wizard will call to actually
        // migrate the data.  It's defined here because we may use it multiple
        // times (with different aProfileDirs), for instance if there is both
        // a Thunderbird and Firefox cal-extension
        function extMigrator(aProfileDir, aCallback) {
            // Get the old datasource
            var dataSource = aProfileDir.clone();
            dataSource.append("CalendarManager.rdf");
            if (!dataSource.exists()) {
                return;
            }

            // Let this be a lesson to anyone designing APIs. The RDF API is so
            // impossibly confusing that it's actually simpler/cleaner/shorter
            // to simply parse as XML and use the better DOM APIs.
            var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                      .createInstance(Ci.nsIXMLHttpRequest);
            req.open('GET', "file://" + dataSource.path, true);
            req.onreadystatechange = function calext_onreadychange() {
                if (req.readyState == 4) {
                    LOG(req.responseText);
                    parseAndMigrate(req.responseXML, aCallback)
                }
            };
            req.send(null);
        }

        // Callback from the XHR above.  Parses CalendarManager.rdf and imports
        // the data describe therein.
        function parseAndMigrate(aDoc, aCallback) {
            // For duplicate detection
            var calManager = getCalendarManager();
            var uris = [];
            for each (var oldCal in calManager.getCalendars({})) {
                uris.push(oldCal.uri);
            }

            function getRDFAttr(aNode, aAttr) {
                return aNode.getAttributeNS("http://home.netscape.com/NC-rdf#",
                                            aAttr);
            }

            const RDFNS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
            var nodes = aDoc.getElementsByTagNameNS(RDFNS, "Description");
            LOG("nodes: " + nodes.length);
            for (var i = 0; i < nodes.length; i++) {
                LOG("Beginning cal node");
                var cal;
                var node = nodes[i];
                if (getRDFAttr(node, "remote") == "false") {
                    LOG("not remote");
                    var localFile = Cc["@mozilla.org/file/local;1"]
                                    .createInstance(Ci.nsILocalFile);
                    localFile.initWithPath(getRDFAttr(node, "path"));
                    cal = gDataMigrator.importICSToStorage(localFile);
                } else {
                    // Remote subscription
                    // XXX check for duplicates
                    var url = makeURL(getRDFAttr(node, "remotePath"));
                    cal = calManager.createCalendar("ics", url);
                }
                cal.name = getRDFAttr(node, "name");
                calManager.setCalendarPref(cal, "color", 
                                           getRDFAttr(node, "color"));
                getCompositeCalendar().addCalendar(cal);
            }
            aCallback();
        }

        var migrators = [];

        // Look in our current profile directory, in case we're upgrading in
        // place
        var profileDir = this.dirService.get("ProfD", Ci.nsILocalFile);
        profileDir.append("Calendar");
        if (profileDir.exists()) {
            LOG("Found old extension directory in current app");
            var title;
            if (this.mIsLightning) {
                title = "Mozilla Calendar Extension";
            } else {
                title = "Sunbird 0.2";
            }
            migrators.push(new dataMigrator(title, extMigrator, [profileDir]));
        }

        // Check the profiles of the various other moz-apps for calendar data
        var profiles = [];

        // Do they use Firefox?
        var ffProf, sbProf, tbProf;
        if ((ffProf = this.getFirefoxProfile())) {
            profiles.push(ffProf);
        }

        if (this.mIsLightning) {
            // If we're lightning, check Sunbird
            if ((sbProf = this.getSunbirdProfile())) {
                profiles.push(sbProf);
            }
        } else {
            // Otherwise, check Thunderbird
            if ((tbProf = this.getThunderbirdProfile())) {
                profiles.push(tbProf);
            }
        }

        // Now check all of the profiles in each of these folders for data
        for each (var prof in profiles) {
            var dirEnum = prof.directoryEntries;
            while (dirEnum.hasMoreElements()) {
                var profile = dirEnum.getNext().QueryInterface(Ci.nsIFile);
                if (profile.isFile()) {
                    continue;
                } else {
                    profile.append("Calendar");
                    if (profile.exists()) {
                        LOG("Found old extension directory at" + profile.path);
                        var title = "Mozilla Calendar";
                        migrators.push(new dataMigrator(title, extMigrator, [profile]));
                    }
                }
            }
        }

        return migrators;
    },

    /**
     * Checks to see if Apple's iCal is installed and offers to migrate any data
     * the user has created in it.
     */
    checkIcal: function gdm_ical() {
        LOG("Checking for ical data");

        function icalMigrate(aDataDir, aCallback) {
            aDataDir.append("Sources");
            var dirs = aDataDir.directoryEntries;
            var calManager = getCalendarManager();

            var i = 1;
            while(dirs.hasMoreElements()) {
                var dataDir = dirs.getNext().QueryInterface(Ci.nsIFile);
                var dataStore = dataDir.clone();
                dataStore.append("corestorage.ics");
                if (!dataStore.exists()) {
                    continue;
                }

                var chars = [];
                var fileStream = Cc["@mozilla.org/network/file-input-stream;1"]
                                 .createInstance(Ci.nsIFileInputStream);

                fileStream.init(dataStore, 0x01, 0444, {});
                var convStream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                                 .getService(Ci.nsIConverterInputStream);
                                 convStream.init(fileStream, 'UTF-8', 0, 0x0000);
                var tmpStr = {};
                var str = "";
                while (convStream.readString(-1, tmpStr)) {
                    str += tmpStr.value;
                }

                // Strip out the timezone definitions, since it makes the file
                // invalid otherwise
                var index = str.indexOf(";TZID=");
                while (index != -1) {
                    var endIndex = str.indexOf(':', index);
                    var otherEnd = str.indexOf(';', index+2);
                    if (otherEnd < endIndex) {
                        endIndex = otherEnd;
                    }
                    var sub = str.substring(index, endIndex);
                    str = str.replace(sub, "", "g");
                    index = str.indexOf(";TZID=");
                }
                var tempFile = gDataMigrator.dirService.get("TmpD", Ci.nsIFile);
                tempFile.append("icalTemp.ics");
                tempFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
                var tempUri = gDataMigrator.ioService.newFileURI(tempFile); 

                var stream = Cc["@mozilla.org/network/file-output-stream;1"]
                             .createInstance(Ci.nsIFileOutputStream);
                stream.init(tempFile, 0x2A, 0600, 0);
                var convStream = Cc["@mozilla.org/intl/converter-output-stream;1"]
                                .getService(Ci.nsIConverterOutputStream);
                convStream.init(stream, 'UTF-8', 0, 0x0000);
                convStream.writeString(str);

                var cal = gDataMigrator.importICSToStorage(tempFile);
                cal.name = "iCalendar"+i;
                i++;
            }
            LOG("icalMig making callback");
            aCallback();
        }
        var profileDir = this.dirService.get("ProfD", Ci.nsILocalFile);
        var icalSpec = profileDir.path;
        var icalFile;
        if (!this.isLightning()) {
            var diverge = icalSpec.indexOf("Sunbird");
            if (diverge == -1) {
                return [];
            }
            icalSpec = icalSpec.substr(0, diverge);
            icalFile = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsILocalFile);
            icalFile.initWithPath(icalSpec);
        } else {
            var diverge = icalSpec.indexOf("Thunderbird");
            if (diverge == -1) {
                return [];
            }
            icalSpec = icalSpec.substr(0, diverge);
            icalFile = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsILocalFile);
            icalFile.initWithPath(icalSpec);
            icalFile.append("Application Support");
        }

        icalFile.append("iCal");
        if (icalFile.exists()) {
            return [new dataMigrator("Apple iCal", icalMigrate, [icalFile])];
        }

        return [];
    },

    /**
     * Checks to see if Evolution is installed and offers to migrate any data
     * stored there.
     */
    checkEvolution: function gdm_evolution() {
        LOG("Checking for evolution data");

        function evoMigrate(aDataDir, aCallback) {
            aDataDir.append("Sources");
            var dirs = aDataDir.directoryEntries;
            var calManager = getCalendarManager();

            var i = 1;
            while(dirs.hasMoreElements()) {
                var dataDir = dirs.getNext().QueryInterface(Ci.nsIFile);
                var dataStore = dataDir.clone();
                dataStore.append("calendar.ics");
                if (!dataStore.exists()) {
                    continue;
                }

                var cal = gDataMigrator.importICSToStorage(dataStore);
                //XXX
                cal.name = "Evolution"+i;
                i++;
            }
            aCallback();
        }

        var profileDir = this.dirService.get("ProfD", Ci.nsILocalFile);
        var evoSpec = profileDir.path;
        var evoFile;
        if (!this.mIsLightning) {
            var diverge = evoSpec.indexOf(".mozilla");
            if (diverge == -1) {
                return [];
            }
            evoSpec = evoSpec.substr(0, diverge);
            evoFile = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsILocalFile);
            evoFile.initWithPath(evoSpec);
        } else {
            var diverge = evoSpec.indexOf(".thunderbird");
            if (diverge == -1) {
                return [];
            }
            evoSpec = evoSpec.substr(0, diverge);
            evoFile = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsILocalFile);
            evoFile.initWithPath(evoSpec);
        }
        evoFile.append(".evolution");
        evoFile.append("calendar");
        evoFile.append("local");
        evoFile.append("system");
        if (evoFile.exists()) {
            return [new dataMigrator("Evolution", evoMigrate, [evoFile])];
        }
        return [];
    },

    importICSToStorage: function migrateIcsStorage(icsFile) {
        var calManager = getCalendarManager();
        var uris = [];
        for each (var oldCal in calManager.getCalendars({})) {
            uris.push(oldCal.uri.spec);
        }
        var uri = 'moz-profile-calendar://?id=';
        var i = 1;
        while (uris.indexOf(uri+i) != -1) {
            i++;
        }

        var cal = calManager.createCalendar("storage", makeURL(uri+i));
        var icsImporter = Cc["@mozilla.org/calendar/import;1?type=ics"]
                          .getService(Ci.calIImporter);

        var inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                          .createInstance(Ci.nsIFileInputStream);
        inputStream.init(icsFile, MODE_RDONLY, 0444, {} );
        var items = icsImporter.importFromStream(inputStream, {});
        // Defined in import-export.js
        putItemsIntoCal(cal, items);

        calManager.registerCalendar(cal);
        getCompositeCalendar().addCalendar(cal);
        return cal;
    },

    /**
     * Helper functions for getting the profile directory of various MozApps
     * (Getting the profile dir is way harder than it should be.)
     *
     * Sunbird:
     *     Unix:     ~jdoe/.mozilla/sunbird/
     *     Windows:  %APPDATA%\Mozilla\Sunbird\Profiles
     *     Mac OS X: ~jdoe/Library/Application Support/Sunbird/Profiles
     *
     * Firefox:
     *     Unix:     ~jdoe/.mozilla/firefox/
     *     Windows:  %APPDATA%\Mozilla\Firefox\Profiles
     *     Mac OS X: ~jdoe/Library/Application Support/Firefox/Profiles
     *
     * Thunderbird:
     *     Unix:     ~jdoe/.thunderbird/
     *     Windows:  %APPDATA%\Thunderbird\Profiles
     *     Mac OS X: ~jdoe/Library/Thunderbird/Profiles
     *
     * Notice that Firefox and Sunbird follow essentially the same pattern, so
     * we group them with getNormalProfile
     */
    getFirefoxProfile: function gdm_getFF() {
        return this.getNormalProfile("Firefox");
    },

    getThunderbirdProfile: function gdm_getTB() {
        var localFile;
        var profileRoot = this.dirService.get("DefProfRt", Ci.nsILocalFile);
        LOG("profileRoot = " + profileRoot.path);
        if (this.mIsLightning) {
            localFile = profileRoot;
        } else {
            // Now it gets ugly
            switch (this.mPlatform) {
                case "darwin": // Mac OS X
                case "winnt":
                    localFile = profileRoot.parent.parent.parent;
                    localFile.append("Thunderbird");
                    localFile.append("Profiles");
                    break;
                default: // Unix
                    localFile = profileRoot.parent.parent;
                    localFile.append(".thunderbird");
            }
        }
        LOG("searching for Thunderbird in " + localFile.path);
        return localFile.exists() ? localFile : null;
    },

    getSunbirdProfile: function gdm_getSB() {
        return this.getNormalProfile("Sunbird");
    },

    getNormalProfile: function gdm_getNorm(aAppName) {
        var localFile;
        var profileRoot = this.dirService.get("DefProfRt", Ci.nsILocalFile);
        LOG("profileRoot = " + profileRoot.path);

        if (this.isLightning()) {  // We're in Thunderbird
            switch (this.mPlatform) {
                case "darwin": // Mac OS X
                    localFile = profileRoot.parent.parent;
                    localFile.append("Application Support");
                    localFile.append(aAppName);
                    localFile.append("Profiles");
                    break;
                case "winnt":
                    localFile = profileRoot.parent.parent;
                    localFile.append("Mozilla");
                    localFile.append(aAppName);
                    localFile.append("Profiles");
                    break;
                default: // Unix
                    localFile = profileRoot.parent;
                    localFile.append(".mozilla");
                    localFile.append(aAppName.toLowerCase());
                    break;
            }
        } else {
            switch (this.mPlatform) {
                // On Mac and Windows, we can just remove the "Sunbird" and
                // replace it with "Firefox" to get to Firefox
                case "darwin": // Mac OS X
                case "winnt":
                    localFile = profileRoot.parent.parent;
                    localFile.append(aAppName);
                    localFile.append("Profiles");
                    break;
                default: // Unix
                    localFile = profileRoot.parent;
                    localFile.append(aAppName.toLowerCase());
                    break;
            }
        }
        LOG("searching for " + aAppName + " in " + localFile.path);
        return localFile.exists() ? localFile : null;
    }
};

function LOG(aString) {
    if (!getPrefSafe("calendar.migration.log", false)) {
        return;
    }
    var consoleService = Cc["@mozilla.org/consoleservice;1"]
                         .getService(Ci.nsIConsoleService);
    consoleService.logStringMessage(aString);
    dump(aString+"\n");
}
