import os
import shutil
import time

from marionette_harness import MarionetteTestCase
from marionette_driver.errors import NoAlertPresentException


class TestFirefoxRefresh(MarionetteTestCase):
    _sandbox = "firefox-refresh"

    _username = "marionette-test-login"
    _password = "marionette-test-password"
    _bookmarkURL = "about:mozilla"
    _bookmarkText = "Some bookmark from Marionette"

    _cookieHost = "firefox-refresh.marionette-test.mozilla.org"
    _cookiePath = "some/cookie/path"
    _cookieName = "somecookie"
    _cookieValue = "some cookie value"

    _historyURL = "http://firefox-refresh.marionette-test.mozilla.org/"
    _historyTitle = "Test visit for Firefox Reset"

    _formHistoryFieldName = "some-very-unique-marionette-only-firefox-reset-field"
    _formHistoryValue = "special-pumpkin-value"

    _formAutofillAvailable = False
    _formAutofillAddressGuid = None

    _expectedURLs = ["about:robots", "about:mozilla"]

    def savePassword(self):
        self.runCode("""
          let myLogin = new global.LoginInfo(
            "test.marionette.mozilla.com",
            "http://test.marionette.mozilla.com/some/form/",
            null,
            arguments[0],
            arguments[1],
            "username",
            "password"
          );
          Services.logins.addLogin(myLogin)
        """, script_args=(self._username, self._password))

    def createBookmark(self):
        self.marionette.execute_script("""
          let url = arguments[0];
          let title = arguments[1];
          PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.bookmarksMenuFolder,
            makeURI(url), 0, title);
        """, script_args=(self._bookmarkURL, self._bookmarkText))

    def createHistory(self):
        error = self.runAsyncCode("""
          // Copied from PlacesTestUtils, which isn't available in Marionette tests.
          let didReturn;
          PlacesUtils.asyncHistory.updatePlaces(
            [{title: arguments[1], uri: makeURI(arguments[0]), visits: [{
                transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
                visitDate: (Date.now() - 5000) * 1000,
                referrerURI: makeURI("about:mozilla"),
              }]
            }],
            {
              handleError(resultCode, place) {
                didReturn = true;
                marionetteScriptFinished("Unexpected error in adding visit: " + resultCode);
              },
              handleResult() {},
              handleCompletion() {
                if (!didReturn) {
                  marionetteScriptFinished(false);
                }
              },
            }
          );
        """, script_args=(self._historyURL, self._historyTitle))
        if error:
            print error

    def createFormHistory(self):
        error = self.runAsyncCode("""
          let updateDefinition = {
            op: "add",
            fieldname: arguments[0],
            value: arguments[1],
            firstUsed: (Date.now() - 5000) * 1000,
          };
          let finished = false;
          global.FormHistory.update(updateDefinition, {
            handleError(error) {
              finished = true;
              marionetteScriptFinished(error);
            },
            handleCompletion() {
              if (!finished) {
                marionetteScriptFinished(false);
              }
            }
          });
        """, script_args=(self._formHistoryFieldName, self._formHistoryValue))
        if error:
          print error

    def createFormAutofill(self):
        if not self._formAutofillAvailable:
            return
        self._formAutofillAddressGuid = self.runAsyncCode("""
          const TEST_ADDRESS_1 = {
            "given-name": "John",
            "additional-name": "R.",
            "family-name": "Smith",
            organization: "World Wide Web Consortium",
            "street-address": "32 Vassar Street\\\nMIT Room 32-G524",
            "address-level2": "Cambridge",
            "address-level1": "MA",
            "postal-code": "02139",
            country: "US",
            tel: "+15195555555",
            email: "user@example.com",
          };
          return global.profileStorage.initialize().then(() => {
            return global.profileStorage.addresses.add(TEST_ADDRESS_1);
          }).then(marionetteScriptFinished);
        """)

    def createCookie(self):
        self.runCode("""
          // Expire in 15 minutes:
          let expireTime = Math.floor(Date.now() / 1000) + 15 * 60;
          Services.cookies.add(arguments[0], arguments[1], arguments[2], arguments[3],
                               true, false, false, expireTime);
        """, script_args=(self._cookieHost, self._cookiePath, self._cookieName, self._cookieValue))

    def createSession(self):
        self.runAsyncCode("""
          const COMPLETE_STATE = Ci.nsIWebProgressListener.STATE_STOP +
                                 Ci.nsIWebProgressListener.STATE_IS_NETWORK;
          let {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
          let expectedURLs = Array.from(arguments[0])
          gBrowser.addTabsProgressListener({
            onStateChange(browser, webprogress, request, flags, status) {
              try {
                request && request.QueryInterface(Ci.nsIChannel);
              } catch (ex) {}
              let uriLoaded = request.originalURI && request.originalURI.spec;
              if ((flags & COMPLETE_STATE == COMPLETE_STATE) && uriLoaded &&
                  expectedURLs.includes(uriLoaded)) {
                TabStateFlusher.flush(browser).then(function() {
                  expectedURLs.splice(expectedURLs.indexOf(uriLoaded), 1);
                  if (!expectedURLs.length) {
                    gBrowser.removeTabsProgressListener(this);
                    marionetteScriptFinished();
                  }
                });
              }
            }
          });
          let expectedTabs = new Set();
          for (let url of expectedURLs) {
            expectedTabs.add(gBrowser.addTab(url));
          }
          // Close any other tabs that might be open:
          let allTabs = Array.from(gBrowser.tabs);
          for (let tab of allTabs) {
            if (!expectedTabs.has(tab)) {
              gBrowser.removeTab(tab);
            }
          }
        """, script_args=(self._expectedURLs,))

    def createSync(self):
        # This script will write an entry to the login manager and create
        # a signedInUser.json in the profile dir.
        self.runAsyncCode("""
          Cu.import("resource://gre/modules/FxAccountsStorage.jsm");
          let storage = new FxAccountsStorageManager();
          let data = {email: "test@test.com", uid: "uid", keyFetchToken: "top-secret"};
          storage.initialize(data);
          storage.finalize().then(marionetteScriptFinished);
        """);

    def checkPassword(self):
        loginInfo = self.marionette.execute_script("""
          let ary = Services.logins.findLogins({},
            "test.marionette.mozilla.com",
            "http://test.marionette.mozilla.com/some/form/",
            null, {});
          return ary.length ? ary : {username: "null", password: "null"};
        """)
        self.assertEqual(len(loginInfo), 1)
        self.assertEqual(loginInfo[0]['username'], self._username)
        self.assertEqual(loginInfo[0]['password'], self._password)

        loginCount = self.marionette.execute_script("""
          return Services.logins.getAllLogins().length;
        """)
        # Note that we expect 2 logins - one from us, one from sync.
        self.assertEqual(loginCount, 2, "No other logins are present")

    def checkBookmark(self):
        titleInBookmarks = self.marionette.execute_script("""
          let url = arguments[0];
          let bookmarkIds = PlacesUtils.bookmarks.getBookmarkIdsForURI(makeURI(url), {}, {});
          return bookmarkIds.length == 1 ? PlacesUtils.bookmarks.getItemTitle(bookmarkIds[0]) : "";
        """, script_args=(self._bookmarkURL,))
        self.assertEqual(titleInBookmarks, self._bookmarkText)

    def checkHistory(self):
        historyResult = self.runAsyncCode("""
          PlacesUtils.history.fetch(arguments[0]).then(pageInfo => {
            if (!pageInfo) {
              marionetteScriptFinished("No visits found");
            } else {
              marionetteScriptFinished(pageInfo);
            }
          }).catch(e => {
            marionetteScriptFinished("Unexpected error in fetching page: " + e);
          });
        """, script_args=(self._historyURL,))
        if type(historyResult) == str:
            self.fail(historyResult)
            return

        self.assertEqual(historyResult['title'], self._historyTitle)

    def checkFormHistory(self):
        formFieldResults = self.runAsyncCode("""
          let results = [];
          global.FormHistory.search(["value"], {fieldname: arguments[0]}, {
            handleError(error) {
              results = error;
            },
            handleResult(result) {
              results.push(result);
            },
            handleCompletion() {
              marionetteScriptFinished(results);
            },
          });
        """, script_args=(self._formHistoryFieldName,))
        if type(formFieldResults) == str:
            self.fail(formFieldResults)
            return

        formFieldResultCount = len(formFieldResults)
        self.assertEqual(formFieldResultCount, 1, "Should have exactly 1 entry for this field, got %d" % formFieldResultCount)
        if formFieldResultCount == 1:
            self.assertEqual(formFieldResults[0]['value'], self._formHistoryValue)

        formHistoryCount = self.runAsyncCode("""
          let count;
          let callbacks = {
            handleResult: rv => count = rv,
            handleCompletion() {
              marionetteScriptFinished(count);
            },
          };
          global.FormHistory.count({}, callbacks);
        """)
        self.assertEqual(formHistoryCount, 1, "There should be only 1 entry in the form history")

    def checkFormAutofill(self):
        if not self._formAutofillAvailable:
            return

        formAutofillResults = self.runAsyncCode("""
          return global.profileStorage.initialize().then(() => {
            return global.profileStorage.addresses.getAll()
          }).then(marionetteScriptFinished);
        """,)
        if type(formAutofillResults) == str:
            self.fail(formAutofillResults)
            return

        formAutofillAddressCount = len(formAutofillResults)
        self.assertEqual(formAutofillAddressCount, 1, "Should have exactly 1 saved address, got %d" % formAutofillAddressCount)
        if formAutofillAddressCount == 1:
            self.assertEqual(formAutofillResults[0]['guid'], self._formAutofillAddressGuid)


    def checkCookie(self):
        cookieInfo = self.runCode("""
          try {
            let cookieEnum = Services.cookies.getCookiesFromHost(arguments[0]);
            let cookie = null;
            while (cookieEnum.hasMoreElements()) {
              let hostCookie = cookieEnum.getNext();
              hostCookie.QueryInterface(Ci.nsICookie2);
              // getCookiesFromHost returns any cookie from the BASE host.
              if (hostCookie.rawHost != arguments[0])
                continue;
              if (cookie != null) {
                return "more than 1 cookie! That shouldn't happen!";
              }
              cookie = hostCookie;
            }
            return {path: cookie.path, name: cookie.name, value: cookie.value};
          } catch (ex) {
            return "got exception trying to fetch cookie: " + ex;
          }
        """, script_args=(self._cookieHost,))
        if not isinstance(cookieInfo, dict):
            self.fail(cookieInfo)
            return
        self.assertEqual(cookieInfo['path'], self._cookiePath)
        self.assertEqual(cookieInfo['value'], self._cookieValue)
        self.assertEqual(cookieInfo['name'], self._cookieName)

    def checkSession(self):
        tabURIs = self.runCode("""
          return [... gBrowser.browsers].map(b => b.currentURI && b.currentURI.spec)
        """)
        self.assertSequenceEqual(tabURIs, ["about:welcomeback"])

        # Dismiss modal dialog if any. This is mainly to dismiss the check for
        # default browser dialog if it shows up.
        try:
          alert = self.marionette.switch_to_alert()
          alert.dismiss()
        except NoAlertPresentException:
          pass

        tabURIs = self.runAsyncCode("""
          let mm = gBrowser.selectedBrowser.messageManager;

          let {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
          window.addEventListener("SSWindowStateReady", function testSSPostReset() {
            window.removeEventListener("SSWindowStateReady", testSSPostReset, false);
            Promise.all(gBrowser.browsers.map(b => TabStateFlusher.flush(b))).then(function() {
              marionetteScriptFinished([... gBrowser.browsers].map(b => b.currentURI && b.currentURI.spec));
            });
          }, false);

          let fs = function() {
            if (content.document.readyState === "complete") {
              content.document.getElementById("errorTryAgain").click();
            } else {
              content.window.addEventListener("load", function(event) {
                content.document.getElementById("errorTryAgain").click();
              }, { once: true });
            }
          };

          mm.loadFrameScript("data:application/javascript,(" + fs.toString() + ")()", true);
        """)
        self.assertSequenceEqual(tabURIs, self._expectedURLs)

    def checkSync(self, hasMigrated):
        result = self.runAsyncCode("""
          Cu.import("resource://gre/modules/FxAccountsStorage.jsm");
          let prefs = new global.Preferences("services.sync.");
          let storage = new FxAccountsStorageManager();
          let result = {};
          storage.initialize();
          storage.getAccountData().then(data => {
            result.accountData = data;
            return storage.finalize();
          }).then(() => {
            result.prefUsername = prefs.get("username");
            marionetteScriptFinished(result);
          }).catch(err => {
            marionetteScriptFinished(err.toString());
          });
        """);
        if type(result) != dict:
            self.fail(result)
            return
        self.assertEqual(result["accountData"]["email"], "test@test.com");
        self.assertEqual(result["accountData"]["uid"], "uid");
        self.assertEqual(result["accountData"]["keyFetchToken"], "top-secret");
        if hasMigrated:
          # This test doesn't actually configure sync itself, so the username
          # pref only exists after migration.
          self.assertEqual(result["prefUsername"], "test@test.com");

    def checkProfile(self, hasMigrated=False):
        self.checkPassword()
        self.checkBookmark()
        self.checkHistory()
        self.checkFormHistory()
        self.checkFormAutofill()
        self.checkCookie()
        self.checkSync(hasMigrated);
        if hasMigrated:
            self.checkSession()

    def createProfileData(self):
        self.savePassword()
        self.createBookmark()
        self.createHistory()
        self.createFormHistory()
        self.createFormAutofill()
        self.createCookie()
        self.createSession()
        self.createSync()

    def setUpScriptData(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        self.runCode("""
          window.global = {};
          global.LoginInfo = Components.Constructor("@mozilla.org/login-manager/loginInfo;1", "nsILoginInfo", "init");
          global.profSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(Ci.nsIToolkitProfileService);
          global.Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;
          global.FormHistory = Cu.import("resource://gre/modules/FormHistory.jsm", {}).FormHistory;
        """)
        self._formAutofillAvailable = self.runCode("""
          try {
            global.profileStorage = Cu.import("resource://formautofill/ProfileStorage.jsm", {}).profileStorage;
          } catch(e) {
            return false;
          }
          return true;
        """)

    def runCode(self, script, *args, **kwargs):
        return self.marionette.execute_script(script,
                                              new_sandbox=False,
                                              sandbox=self._sandbox,
                                              *args,
                                              **kwargs)

    def runAsyncCode(self, script, *args, **kwargs):
        return self.marionette.execute_async_script(script,
                                                    new_sandbox=False,
                                                    sandbox=self._sandbox,
                                                    *args,
                                                    **kwargs)

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.setUpScriptData()

        self.reset_profile_path = None
        self.desktop_backup_path = None

        self.createProfileData()

    def tearDown(self):
        # Force yet another restart with a clean profile to disconnect from the
        # profile and environment changes we've made, to leave a more or less
        # blank slate for the next person.
        self.marionette.restart(clean=True, in_app=False)
        self.setUpScriptData()

        # Super
        MarionetteTestCase.tearDown(self)

        # Some helpers to deal with removing a load of files
        import errno, stat
        def handleRemoveReadonly(func, path, exc):
            excvalue = exc[1]
            if func in (os.rmdir, os.remove) and excvalue.errno == errno.EACCES:
                os.chmod(path, stat.S_IRWXU| stat.S_IRWXG| stat.S_IRWXO) # 0777
                func(path)
            else:
                raise

        if self.desktop_backup_path:
            shutil.rmtree(self.desktop_backup_path, ignore_errors=False, onerror=handleRemoveReadonly)

        if self.reset_profile_path:
            # Remove ourselves from profiles.ini
            self.runCode("""
              let name = arguments[0];
              let profile = global.profSvc.getProfileByName(name);
              profile.remove(false)
              global.profSvc.flush();
            """, script_args=(self.profileNameToRemove,))
            # And delete all the files.
            shutil.rmtree(self.reset_profile_path, ignore_errors=False, onerror=handleRemoveReadonly)

    def doReset(self):
        profileName = "marionette-test-profile-" + str(int(time.time() * 1000))
        self.profileNameToRemove = profileName
        self.runCode("""
          // Ensure the current (temporary) profile is in profiles.ini:
          let profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
          let profileName = arguments[1];
          let myProfile = global.profSvc.createProfile(profD, profileName);
          global.profSvc.flush()

          // Now add the reset parameters:
          let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
          let prefsToKeep = Array.from(Services.prefs.getChildList("marionette."));
          prefsToKeep.push("datareporting.policy.dataSubmissionPolicyBypassNotification");
          let prefObj = {};
          for (let pref of prefsToKeep) {
            prefObj[pref] = global.Preferences.get(pref);
          }
          env.set("MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS", JSON.stringify(prefObj));
          env.set("MOZ_RESET_PROFILE_RESTART", "1");
          env.set("XRE_PROFILE_PATH", arguments[0]);
          env.set("XRE_PROFILE_NAME", profileName);
        """, script_args=(self.marionette.instance.profile.profile, profileName,))

        profileLeafName = os.path.basename(os.path.normpath(self.marionette.instance.profile.profile))

        # Now restart the browser to get it reset:
        self.marionette.restart(clean=False, in_app=True)
        self.setUpScriptData()

        # Determine the new profile path (we'll need to remove it when we're done)
        self.reset_profile_path = self.runCode("""
          let profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
          return profD.path;
        """)

        # Determine the backup path
        self.desktop_backup_path = self.runCode("""
          let container;
          try {
            container = Services.dirsvc.get("Desk", Ci.nsIFile);
          } catch (ex) {
            container = Services.dirsvc.get("Home", Ci.nsIFile);
          }
          let bundle = Services.strings.createBundle("chrome://mozapps/locale/profile/profileSelection.properties");
          let dirName = bundle.formatStringFromName("resetBackupDirectory", [Services.appinfo.name], 1);
          container.append(dirName);
          container.append(arguments[0]);
          return container.path;
        """, script_args=(profileLeafName,))

        self.assertTrue(os.path.isdir(self.reset_profile_path), "Reset profile path should be present")
        self.assertTrue(os.path.isdir(self.desktop_backup_path), "Backup profile path should be present")
        self.assertTrue(self.profileNameToRemove in self.reset_profile_path, "Reset profile path should contain profile name to remove")

    def testReset(self):
        self.checkProfile()

        self.doReset()

        # Now check that we're doing OK...
        self.checkProfile(hasMigrated=True)
