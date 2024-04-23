# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

from marionette_harness import MarionetteTestCase


class BackupTest(MarionetteTestCase):
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

        # In bug 1892105, we'll update this test to write some values to various
        # datastores, like bookmarks, passwords, cookies, etc. Then we'll run
        # a backup and ensure that the data can be recovered from the backup.
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

        stagingPath = self.marionette.execute_async_script(
            """
          const { BackupService } = ChromeUtils.importESModule("resource:///modules/backup/BackupService.sys.mjs");
          let bs = BackupService.init();
          if (!bs) {
            throw new Error("Could not get initialized BackupService.");
          }

          let [outerResolve] = arguments;
          (async () => {
            let { stagingPath } = await bs.createBackup();
            if (!stagingPath) {
              throw new Error("Could not create backup.");
            }
            return stagingPath;
          })().then(outerResolve);
        """
        )

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

        # In bug 1892105, we'll update this test to then recover from this
        # staging directory and ensure that the recovered data is what we
        # expect.
