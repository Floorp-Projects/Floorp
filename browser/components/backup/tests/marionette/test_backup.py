# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import shutil
import tempfile

import mozfile
from marionette_harness import MarionetteTestCase


class BackupTest(MarionetteTestCase):
    # This is the DB key that will be computed for the http2-ca.pem certificate
    # that's included in a support-file for this test.
    _cert_db_key = "AAAAAAAAAAAAAAAUAAAAG0Wbze8lahTcE4RhwEqMtTpThrzjMBkxFzAVBgNVBAMMDiBIVFRQMiBUZXN0IENB"

    def setUp(self):
        MarionetteTestCase.setUp(self)
        # We need to quit the browser and restart with the browser.backup.log
        # pref already set to true in order for it to be displayed.
        self.marionette.quit()
        self.marionette.instance.prefs = {
            "browser.backup.log": True,
        }
        # Now restart the browser.
        self.marionette.instance.switch_profile()
        self.marionette.start_session()

    def test_backup(self):
        self.marionette.set_context("chrome")

        self.add_test_cookie()
        self.add_test_login()
        self.add_test_certificate()
        self.add_test_saved_address()
        self.add_test_identity_credential()
        self.add_test_form_history()
        self.add_test_activity_stream_snippets_data()
        self.add_test_protections_data()
        self.add_test_bookmarks()
        self.add_test_history()
        self.add_test_preferences()
        self.add_test_permissions()

        resourceKeys = self.marionette.execute_script(
            """
          const DefaultBackupResources = ChromeUtils.importESModule("resource:///modules/backup/BackupResources.sys.mjs");
          let resourceKeys = [];
          for (const resourceName in DefaultBackupResources) {
            let resource = DefaultBackupResources[resourceName];
            resourceKeys.push(resource.key);
          }
          return resourceKeys;
        """
        )

        originalStagingPath = self.marionette.execute_async_script(
            """
          const { OSKeyStore } = ChromeUtils.importESModule(
            "resource://gre/modules/OSKeyStore.sys.mjs"
          );
          const { BackupService } = ChromeUtils.importESModule("resource:///modules/backup/BackupService.sys.mjs");
          let bs = BackupService.init();
          if (!bs) {
            throw new Error("Could not get initialized BackupService.");
          }

          let [outerResolve] = arguments;
          (async () => {
            // This is some hackery to make it so that OSKeyStore doesn't kick
            // off an OS authentication dialog in our test, and also to make
            // sure we don't blow away the _real_ OSKeyStore key for the browser
            // on the system that this test is running on. Normally, I'd use
            // OSKeyStoreTestUtils.setup to do this, but apparently the
            // testing-common modules aren't available in Marionette tests.
            const ORIGINAL_STORE_LABEL = OSKeyStore.STORE_LABEL;
            OSKeyStore.STORE_LABEL = "test-" + Math.random().toString(36).substr(2);
            await bs.enableEncryption("This is a test password");
            await OSKeyStore.cleanup();
            OSKeyStore.STORE_LABEL = ORIGINAL_STORE_LABEL;

            let { stagingPath } = await bs.createBackup();
            if (!stagingPath) {
              throw new Error("Could not create backup.");
            }
            return stagingPath;
          })().then(outerResolve);
        """
        )

        # When we switch over to the recovered profile, the Marionette framework
        # will blow away the profile directory of the one that we created the
        # backup on, which ruins our ability to do postRecovery work, since
        # that relies on the prior profile sticking around. We work around this
        # by moving the staging folder we got back to the OS temporary
        # directory, and telling the recovery method to use that instead of the
        # one from the profile directory.
        stagingPath = os.path.join(tempfile.gettempdir(), "staging-test")
        # Delete the destination folder if it exists already
        shutil.rmtree(stagingPath, ignore_errors=True)
        shutil.move(originalStagingPath, stagingPath)

        # First, ensure that the staging path exists
        self.assertTrue(os.path.exists(stagingPath))
        # Now, ensure that the backup-manifest.json file exists within it.
        manifestPath = os.path.join(stagingPath, "backup-manifest.json")
        self.assertTrue(os.path.exists(manifestPath))

        # For now, we just do a cursory check to ensure that for the resources
        # that are listed in the manifest as having been backed up, that we
        # have at least one file in their respective staging directories.
        # We don't check the contents of the files, just that they exist.

        # Read the JSON manifest file
        with open(manifestPath, "r") as f:
            manifest = json.load(f)

        # Ensure that the manifest has a "resources" key
        self.assertIn("resources", manifest)
        resources = manifest["resources"]
        self.assertTrue(isinstance(resources, dict))
        self.assertTrue(len(resources) > 0)

        # We don't have encryption capabilities wired up yet, so we'll check
        # that all default resources are represented in the manifest.
        self.assertEqual(len(resources), len(resourceKeys))
        for resourceKey in resourceKeys:
            self.assertIn(resourceKey, resources)

        # Iterate the resources dict keys
        for resourceKey in resources:
            print("Checking resource: %s" % resourceKey)
            # Ensure that there are staging directories created for each
            # resource that was backed up
            resourceStagingDir = os.path.join(stagingPath, resourceKey)
            self.assertTrue(os.path.exists(resourceStagingDir))

        # Start a brand new profile, one without any of the data we created or
        # backed up. This is the one that we'll be starting recovery from.
        self.marionette.quit()
        self.marionette.instance.profile = None
        self.marionette.start_session()
        self.marionette.set_context("chrome")

        # Recover the created backup into a new profile directory. Also get out
        # the client ID of this profile, because we're going to want to make
        # sure that this client ID is inherited by the recovered profile.
        [
            newProfileName,
            newProfilePath,
            expectedClientID,
        ] = self.marionette.execute_async_script(
            """
          const { ClientID } = ChromeUtils.importESModule("resource://gre/modules/ClientID.sys.mjs");
          const { BackupService } = ChromeUtils.importESModule("resource:///modules/backup/BackupService.sys.mjs");
          let bs = BackupService.get();
          if (!bs) {
            throw new Error("Could not get initialized BackupService.");
          }

          let [stagingPath, outerResolve] = arguments;
          (async () => {
            let newProfileRootPath = await IOUtils.createUniqueDirectory(
              PathUtils.tempDir,
              "recoverFromBackupTest-newProfileRoot"
            );
            let newProfile = await bs.recoverFromBackup(stagingPath, false, newProfileRootPath)
            if (!newProfile) {
              throw new Error("Could not create recovery profile.");
            }

            let expectedClientID = await ClientID.getClientID();

            return [newProfile.name, newProfile.rootDir.path, expectedClientID];
          })().then(outerResolve);
        """,
            script_args=[stagingPath],
        )

        print("Recovery name: %s" % newProfileName)
        print("Recovery path: %s" % newProfilePath)
        print("Expected clientID: %s" % expectedClientID)

        self.marionette.quit()
        originalProfile = self.marionette.instance.profile
        self.marionette.instance.profile = newProfilePath
        self.marionette.start_session()
        self.marionette.set_context("chrome")

        # Ensure that all postRecovery actions have completed.
        self.marionette.execute_async_script(
            """
          const { BackupService } = ChromeUtils.importESModule("resource:///modules/backup/BackupService.sys.mjs");
          let bs = BackupService.get();
          if (!bs) {
            throw new Error("Could not get initialized BackupService.");
          }

          let [outerResolve] = arguments;
          (async () => {
            await bs.postRecoveryComplete;
          })().then(outerResolve);
        """
        )

        self.verify_recovered_test_cookie()
        self.verify_recovered_test_login()
        self.verify_recovered_test_certificate()
        self.verify_recovered_saved_address()
        self.verify_recovered_identity_credential()
        self.verify_recovered_form_history()
        self.verify_recovered_activity_stream_snippets_data()
        self.verify_recovered_protections_data()
        self.verify_recovered_bookmarks()
        self.verify_recovered_history()
        self.verify_recovered_preferences()
        self.verify_recovered_permissions()

        # Now also ensure that the recovered profile inherited the client ID
        # from the profile that initiated recovery.
        recoveredClientID = self.marionette.execute_async_script(
            """
          const { ClientID } = ChromeUtils.importESModule("resource://gre/modules/ClientID.sys.mjs");
          let [outerResolve] = arguments;
          (async () => {
            return ClientID.getClientID();
          })().then(outerResolve);
        """
        )
        self.assertEqual(recoveredClientID, expectedClientID)

        # Try not to pollute the profile list by getting rid of the one we just
        # created.
        self.marionette.quit()
        self.marionette.instance.profile = originalProfile
        self.marionette.start_session()
        self.marionette.set_context("chrome")
        self.marionette.execute_script(
            """
          let newProfileName = arguments[0];
          let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
            Ci.nsIToolkitProfileService
          );
          let profile = profileSvc.getProfileByName(newProfileName);
          profile.remove(true);
          profileSvc.flush();
        """,
            script_args=[newProfileName],
        )

        # Cleanup the staging path that we moved
        mozfile.remove(stagingPath)

    def add_test_cookie(self):
        self.marionette.execute_async_script(
            """
          let [outerResolve] = arguments;
          (async () => {
            // We'll just add a single cookie, and then make sure that it shows
            // up on the other side.
            Services.cookies.removeAll();
            Services.cookies.add(
              ".example.com",
              "/",
              "first",
              "one",
              false,
              false,
              false,
              Date.now() / 1000 + 1,
              {},
              Ci.nsICookie.SAMESITE_NONE,
              Ci.nsICookie.SCHEME_HTTP
            );
          })().then(outerResolve);
        """
        )

    def verify_recovered_test_cookie(self):
        cookiesLength = self.marionette.execute_async_script(
            """
          let [outerResolve] = arguments;
          (async () => {
            let cookies = Services.cookies.getCookiesFromHost("example.com", {});
            return cookies.length;
          })().then(outerResolve);
        """
        )
        self.assertEqual(cookiesLength, 1)

    def add_test_login(self):
        self.marionette.execute_async_script(
            """
          let [outerResolve] = arguments;
          (async () => {
            // Let's start with adding a single password
            Services.logins.removeAllLogins();

            const nsLoginInfo = new Components.Constructor(
              "@mozilla.org/login-manager/loginInfo;1",
              Ci.nsILoginInfo,
              "init"
            );

            const login1 = new nsLoginInfo(
              "https://example.com",
              "https://example.com",
              null,
              "notifyu1",
              "notifyp1",
              "user",
              "pass"
            );
            await Services.logins.addLoginAsync(login1);
          })().then(outerResolve);
        """
        )

    def verify_recovered_test_login(self):
        loginsLength = self.marionette.execute_async_script(
            """
          let [outerResolve] = arguments;
          (async () => {
            let logins = await Services.logins.searchLoginsAsync({
              origin: "https://example.com",
            });
            return logins.length;
          })().then(outerResolve);
        """
        )
        self.assertEqual(loginsLength, 1)

    def add_test_certificate(self):
        certPath = os.path.join(os.path.dirname(__file__), "http2-ca.pem")
        self.marionette.execute_async_script(
            """
          let [certPath, certDbKey, outerResolve] = arguments;
          (async () => {
            const { NetUtil } = ChromeUtils.importESModule(
              "resource://gre/modules/NetUtil.sys.mjs"
            );

            let certDb = Cc["@mozilla.org/security/x509certdb;1"].getService(
              Ci.nsIX509CertDB
            );

            if (certDb.findCertByDBKey(certDbKey)) {
              throw new Error("Should not have this certificate yet!");
            }

            let certFile = await IOUtils.getFile(certPath);
            let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
              Ci.nsIFileInputStream
            );
            fstream.init(certFile, -1, 0, 0);
            let data = NetUtil.readInputStreamToString(fstream, fstream.available());
            fstream.close();

            let pem = data.replace(/-----BEGIN CERTIFICATE-----/, "")
                          .replace(/-----END CERTIFICATE-----/, "")
                          .replace(/[\\r\\n]/g, "");
            let cert = certDb.addCertFromBase64(pem, "CTu,u,u");

            if (cert.dbKey != certDbKey) {
              throw new Error("The inserted certificate DB key is unexpected.");
            }
          })().then(outerResolve);
        """,
            script_args=[certPath, self._cert_db_key],
        )

    def verify_recovered_test_certificate(self):
        certExists = self.marionette.execute_async_script(
            """
          let [certDbKey, outerResolve] = arguments;
          (async () => {
            let certDb = Cc["@mozilla.org/security/x509certdb;1"].getService(
              Ci.nsIX509CertDB
            );
            return certDb.findCertByDBKey(certDbKey) != null;
          })().then(outerResolve);
        """,
            script_args=[self._cert_db_key],
        )
        self.assertTrue(certExists)

    def add_test_saved_address(self):
        self.marionette.execute_async_script(
            """
          const { formAutofillStorage } = ChromeUtils.importESModule(
            "resource://autofill/FormAutofillStorage.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
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
            await formAutofillStorage.initialize();
            formAutofillStorage.addresses.removeAll();
            await formAutofillStorage.addresses.add(TEST_ADDRESS_1);
          })().then(outerResolve);
        """
        )

    def verify_recovered_saved_address(self):
        addressesLength = self.marionette.execute_async_script(
            """
          const { formAutofillStorage } = ChromeUtils.importESModule(
            "resource://autofill/FormAutofillStorage.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            await formAutofillStorage.initialize();
            let addresses = await formAutofillStorage.addresses.getAll();
            return addresses.length;
          })().then(outerResolve);
        """
        )
        self.assertEqual(addressesLength, 1)

    def add_test_identity_credential(self):
        self.marionette.execute_async_script(
            """
          let [outerResolve] = arguments;
          (async () => {
            let service = Cc["@mozilla.org/browser/identity-credential-storage-service;1"]
                            .getService(Ci.nsIIdentityCredentialStorageService);
            service.clear();

            let testPrincipal = Services.scriptSecurityManager.createContentPrincipal(
              Services.io.newURI("https://test.com/"),
              {}
            );
            let idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
              Services.io.newURI("https://idp-test.com/"),
              {}
            );

            service.setState(
              testPrincipal,
              idpPrincipal,
              "ID",
              true,
              true
            );

          })().then(outerResolve);
        """
        )

    def verify_recovered_identity_credential(self):
        [registered, allowLogout] = self.marionette.execute_async_script(
            """
          let [outerResolve] = arguments;
          (async () => {
            let service = Cc["@mozilla.org/browser/identity-credential-storage-service;1"]
                            .getService(Ci.nsIIdentityCredentialStorageService);

            let testPrincipal = Services.scriptSecurityManager.createContentPrincipal(
              Services.io.newURI("https://test.com/"),
              {}
            );
            let idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
              Services.io.newURI("https://idp-test.com/"),
              {}
            );

            let registered = {};
            let allowLogout = {};

            service.getState(
              testPrincipal,
              idpPrincipal,
              "ID",
              registered,
              allowLogout
            );

            return [registered.value, allowLogout.value];
          })().then(outerResolve);
        """
        )
        self.assertTrue(registered)
        self.assertTrue(allowLogout)

    def add_test_form_history(self):
        self.marionette.execute_async_script(
            """
          const { FormHistory } = ChromeUtils.importESModule(
            "resource://gre/modules/FormHistory.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            await FormHistory.update({
              op: "add",
              fieldname: "some-test-field",
              value: "I was recovered!",
              timesUsed: 1,
              firstUsed: 0,
              lastUsed: 0,
            });

          })().then(outerResolve);
        """
        )

    def verify_recovered_form_history(self):
        formHistoryResultsLength = self.marionette.execute_async_script(
            """
          const { FormHistory } = ChromeUtils.importESModule(
            "resource://gre/modules/FormHistory.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            let results = await FormHistory.search(
              ["guid"],
              { fieldname: "some-test-field" }
            );
            return results.length;
          })().then(outerResolve);
        """
        )
        self.assertEqual(formHistoryResultsLength, 1)

    def add_test_activity_stream_snippets_data(self):
        self.marionette.execute_async_script(
            """
          const { ActivityStreamStorage } = ChromeUtils.importESModule(
            "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs",
          );
          const SNIPPETS_TABLE_NAME = "snippets";

          let [outerResolve] = arguments;
          (async () => {
            let storage = new ActivityStreamStorage({
              storeNames: [SNIPPETS_TABLE_NAME],
            });
            let snippetsTable = await storage.getDbTable(SNIPPETS_TABLE_NAME);
            await snippetsTable.set("backup-test", "some-test-value");
          })().then(outerResolve);
        """
        )

    def verify_recovered_activity_stream_snippets_data(self):
        snippetsResult = self.marionette.execute_async_script(
            """
          const { ActivityStreamStorage } = ChromeUtils.importESModule(
            "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs",
          );
          const SNIPPETS_TABLE_NAME = "snippets";

          let [outerResolve] = arguments;
          (async () => {
            let storage = new ActivityStreamStorage({
              storeNames: [SNIPPETS_TABLE_NAME],
            });
            let snippetsTable = await storage.getDbTable(SNIPPETS_TABLE_NAME);
            return await snippetsTable.get("backup-test");
          })().then(outerResolve);
        """
        )
        self.assertEqual(snippetsResult, "some-test-value")

    def add_test_protections_data(self):
        self.marionette.execute_async_script(
            """
          const TrackingDBService = Cc["@mozilla.org/tracking-db-service;1"]
                                      .getService(Ci.nsITrackingDBService);

          let [outerResolve] = arguments;
          (async () => {
            let entry = {
              "https://test.com": [
                [Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT, true, 1],
              ],
            };
            await TrackingDBService.clearAll();
            await TrackingDBService.saveEvents(JSON.stringify(entry));
          })().then(outerResolve);
        """
        )

    def verify_recovered_protections_data(self):
        eventsSum = self.marionette.execute_async_script(
            """
          const TrackingDBService = Cc["@mozilla.org/tracking-db-service;1"]
                                      .getService(Ci.nsITrackingDBService);

          let [outerResolve] = arguments;
          (async () => {
            return TrackingDBService.sumAllEvents();
          })().then(outerResolve);
        """
        )
        self.assertEqual(eventsSum, 1)

    def add_test_bookmarks(self):
        self.marionette.execute_async_script(
            """
          const { PlacesUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/PlacesUtils.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            await PlacesUtils.bookmarks.eraseEverything();
            await PlacesUtils.bookmarks.insert({
              parentGuid: PlacesUtils.bookmarks.toolbarGuid,
              title: "Some test page",
              url: Services.io.newURI("https://www.backup.test/"),
            });
          })().then(outerResolve);
        """
        )

    def verify_recovered_bookmarks(self):
        bookmarkExists = self.marionette.execute_async_script(
            """
          const { PlacesUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/PlacesUtils.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            let url = Services.io.newURI("https://www.backup.test/");
            let bookmark = await PlacesUtils.bookmarks.fetch({ url });
            return bookmark != null;
          })().then(outerResolve);
        """
        )
        self.assertTrue(bookmarkExists)

    def add_test_history(self):
        self.marionette.execute_async_script(
            """
          const { PlacesUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/PlacesUtils.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            await PlacesUtils.history.clear();

            let entry = {
              url: "http://my-restored-history.com",
              visits: [{ transition: PlacesUtils.history.TRANSITION_LINK }],
            };

            await PlacesUtils.history.insertMany([entry]);
          })().then(outerResolve);
        """
        )

    def verify_recovered_history(self):
        historyExists = self.marionette.execute_async_script(
            """
          const { PlacesUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/PlacesUtils.sys.mjs"
          );

          let [outerResolve] = arguments;
          (async () => {
            let entry = await PlacesUtils.history.fetch("http://my-restored-history.com");
            return entry != null;
          })().then(outerResolve);
        """
        )
        self.assertTrue(historyExists)

    def add_test_preferences(self):
        self.marionette.execute_script(
            """
          Services.prefs.setBoolPref("test-pref-for-backup", true)
        """
        )

    def verify_recovered_preferences(self):
        prefExists = self.marionette.execute_script(
            """
          return Services.prefs.getBoolPref("test-pref-for-backup", false);
        """
        )
        self.assertTrue(prefExists)

    def add_test_permissions(self):
        self.marionette.execute_script(
            """
          let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://test-permission-site.com"
          );
          Services.perms.addFromPrincipal(
            principal,
            "desktop-notification",
            Services.perms.ALLOW_ACTION
          );
        """
        )

    def verify_recovered_permissions(self):
        permissionExists = self.marionette.execute_script(
            """
          let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://test-permission-site.com"
          );
          let perms = Services.perms.getAllForPrincipal(principal);
          if (perms.length != 1) {
            throw new Error("Got an unexpected number of permissions");
          }
          return perms[0].type == "desktop-notification"
        """
        )
        self.assertTrue(permissionExists)
