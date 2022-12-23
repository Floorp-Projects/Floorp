# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil

from marionette_harness import MarionetteTestCase


class TestAutoConfig(MarionetteTestCase):
    def tearDown(self):
        self.marionette.quit(in_app=False, clean=True)

        if hasattr(self, "pref_file"):
            os.remove(self.pref_file)
        if hasattr(self, "autoconfig_file"):
            os.remove(self.autoconfig_file)

        super(TestAutoConfig, self).tearDown()

    def pref_has_user_value(self, pref):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                """
                return Services.prefs.prefHasUserValue(arguments[0]);
                """,
                script_args=(pref,),
            )

    def pref_is_locked(self, pref):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                """
                return Services.prefs.prefIsLocked(arguments[0]);
                """,
                script_args=(pref,),
            )

    def test_autoconfig(self):
        with self.marionette.using_context("chrome"):
            self.exe_dir = self.marionette.execute_script(
                """
              return Services.dirsvc.get("GreD", Ci.nsIFile).path;
            """
            )

        self.marionette.quit()

        test_dir = os.path.dirname(__file__)
        self.pref_file = os.path.join(self.exe_dir, "defaults", "pref", "autoconfig.js")
        shutil.copyfile(os.path.join(test_dir, "autoconfig.js"), self.pref_file)
        self.autoconfig_file = os.path.join(self.exe_dir, "autoconfig.cfg")
        shutil.copyfile(os.path.join(test_dir, "autoconfig.cfg"), self.autoconfig_file)

        self.marionette.start_session()

        with self.marionette.using_context("chrome"):
            self.assertTrue(
                self.pref_has_user_value("_autoconfig_.test.userpref"),
                "Pref should have user value",
            )

            self.assertEqual(
                self.marionette.get_pref("_autoconfig_.test.userpref"),
                "userpref",
                "User pref should be set",
            )

            self.assertEqual(
                self.marionette.get_pref("_autoconfig_.test.defaultpref", True),
                "defaultpref",
                "Default pref should be set",
            )

            self.assertTrue(
                self.pref_is_locked("_autoconfig_.test.lockpref"),
                "Pref should be locked",
            )

            self.assertEqual(
                self.marionette.get_pref("_autoconfig_.test.lockpref"),
                "lockpref",
                "Locked pref should be set",
            )

            self.assertFalse(
                self.pref_is_locked("_autoconfig_.test.unlockpref"),
                "Pref should be unlocked",
            )

            self.assertEqual(
                self.marionette.get_pref("_autoconfig_.test.unlockpref"),
                "unlockpref",
                "Unlocked pref should be set",
            )

            self.assertFalse(
                self.pref_has_user_value("_autoconfig_.test.clearpref"),
                "Pref should be cleared",
            )
