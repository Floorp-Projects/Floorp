# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import platform
from unittest import skipIf

from marionette_driver.by import By
from marionette_harness.marionette_test import MarionetteTestCase


gfx_rollout_override = 'gfx.webrender.all.qualified.gfxPref-default-override'
hw_qualified_override = 'gfx.webrender.all.qualified.hardware-override'
rollout_pref = 'gfx.webrender.all.qualified'

class WrPrefRolloutWorkAroundTestCase(MarionetteTestCase):
    '''Test cases for WebRender gradual pref rollout work around.
       Normandy sets default prefs when rolling out a pref change, but
       gfx starts up before Normandy can set the pref's default value
       so we save the default value on shutdown, and check it on startup.
       This test verifies that we save and load the default value,
       and that the right compositor is enabled due to the rollout.
    '''

    def test_wr_rollout_workaround_on_non_qualifying_hw(self):
        # Override the gfxPref so that WR is not enabled, as it would be before a rollout.
        self.marionette.set_pref(pref=gfx_rollout_override, value=False)
        # Set HW override so we behave as if we on non-qualifying hardware.
        self.marionette.set_pref(pref=hw_qualified_override, value=False)
        # Ensure we don't fallback to the basic compositor for some spurious reason.
        self.marionette.set_pref(pref='layers.acceleration.force-enabled', value=True)

        # Restart browser. Gfx will observe hardware qualification override, and
        # gfx rollout override prefs. We should then be running in a browser which
        # behaves as if it's running on hardware that does not qualify for WR,
        # with WR not rolled out yet.
        self.marionette.restart(clean=False, in_app=True)

        # Ensure we're not yet using WR; we're not rolled out yet!
        status, compositor = self.wr_status()
        print('self.wr_status()={},{}'.format(status, compositor))
        self.assertEqual(status, 'opt-in', 'Should start out as WR opt-in')
        self.assertTrue(compositor != 'webrender', 'Before WR rollout on non-qualifying HW, should not be using WR.')

        # Set the rollout pref's default value, as Normandy would do, and restart.
        # Gfx's shutdown observer should save the default value of the pref. Upon
        # restart, because we're pretending to be on *non-qualifying* hardware, WR
        # should be reported as blocked.
        self.marionette.set_pref(pref=rollout_pref, value=True, default_branch=True)
        self.marionette.restart(clean=False, in_app=True)
        status, compositor = self.wr_status()
        print('self.wr_status()={},{}'.format(status, compositor))
        self.assertEqual(status, 'opt-in', 'WR rolled out on non-qualifying hardware should not use WR.')
        self.assertTrue(compositor != 'webrender', 'WR rolled out on non-qualifying HW should not be used.')

        # Simulate a rollback of the rollout; set the pref to false at runtime.
        self.marionette.set_pref(pref=rollout_pref, value=False, default_branch=True)
        self.marionette.restart(clean=False, in_app=True)
        status, compositor = self.wr_status()
        print('self.wr_status()={},{}'.format(status, compositor))
        self.assertEqual(status, 'opt-in', 'WR rollback of rollout should revert to opt-in on non-qualifying hardware.')
        self.assertTrue(compositor != 'webrender', 'After roll back on non-qualifying HW, WR should not be used.')

    @skipIf(platform.machine() == "ARM64" and platform.system() == "Windows", "Bug 1536369 - Crashes on Windows 10 aarch64")
    def test_wr_rollout_workaround_on_qualifying_hw(self):
        # Override the gfxPref so that WR is not enabled, as it would be before a rollout.
        self.marionette.set_pref(pref=gfx_rollout_override, value=False)
        # Set HW override so we behave as if we on qualifying hardware.
        self.marionette.set_pref(pref=hw_qualified_override, value=True)
        self.marionette.set_pref(pref='layers.acceleration.force-enabled', value=True)

        # Restart browser. Gfx will observe hardware qualification override, and
        # gfx rollout override prefs. We should then be running in a browser which
        # behaves as if it's running on *qualifying* hardware, with WR not rolled
        # out yet.
        self.marionette.restart(clean=False, in_app=True)

        # Ensure we're not yet using WR; we're not rolled out yet!
        status, compositor = self.wr_status()
        print('self.wr_status()={},{}'.format(status, compositor))
        self.assertEqual(status, 'opt-in', 'Should start out as WR opt-in')
        self.assertTrue(compositor != 'webrender', 'Before WR rollout on qualifying HW, should not be using WR.')

        # Set the rollout pref's default value, as Normandy would do, and restart.
        # Gfx's shutdown observer should save the default value of the pref. Upon
        # restart, because we're pretending to be on *qualifying* hardware, WR
        # should be reported as available.
        self.marionette.set_pref(pref=rollout_pref, value=True, default_branch=True)
        self.marionette.restart(clean=False, in_app=True)
        status, compositor = self.wr_status()
        print('self.wr_status()={},{}'.format(status, compositor))
        self.assertEqual(status, 'available', 'WR rolled out on qualifying hardware should report be available #1.')
        self.assertEqual(compositor, 'webrender', 'After rollout on qualifying HW, WR should be used.')

        # Simulate a rollback of the rollout; set the pref to false at runtime.
        self.marionette.set_pref(pref=rollout_pref, value=False, default_branch=True)
        self.marionette.restart(clean=False, in_app=True)
        status, compositor = self.wr_status()
        print('self.wr_status()={},{}'.format(status, compositor))
        self.assertEqual(status, 'opt-in', 'WR rollback of rollout should revert to opt-in on qualifying hardware.')
        self.assertTrue(compositor != 'webrender', 'After roll back on qualifying HW, WR should not be used.')

    def wr_status(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        result = self.marionette.execute_script('''
            try {
                const gfxInfo = Components.classes['@mozilla.org/gfx/info;1'].getService(Ci.nsIGfxInfo);
                return {features: gfxInfo.getFeatures(), log: gfxInfo.getFeatureLog()};
            } catch (e) {
                return {}
            }
        ''')
        return result['features']['webrender']['status'], result['features']['compositor']
