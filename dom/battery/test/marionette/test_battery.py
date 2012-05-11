import unittest
from marionette_test import MarionetteTestCase


class BatteryTest(MarionetteTestCase):

    # TODO disabled for now since the failure messes up the test harness somehow.
    @unittest.expectedFailure
    def dont_test_chargingchange(self):
        marionette = self.marionette
        self.assertTrue(marionette.emulator.is_running)
        marionette.set_script_timeout(10000)

        moz_charging = marionette.execute_script("return navigator.mozBattery.charging;",
                                                 new_sandbox=False)
        emulator_charging = marionette.emulator.battery.charging
        self.assertEquals(moz_charging, emulator_charging)

        # setup event listeners to be notified when the level or charging status 
        # changes
        self.assertTrue(marionette.execute_script("""
        global.chargingchanged = false;
        navigator.mozBattery.addEventListener("chargingchange", function() {
            global.chargingchanged = true;
        });
        return true;
        """, new_sandbox=False))

        # set the battery charging state, and verify
        marionette.emulator.battery.charging = not emulator_charging
        new_emulator_charging_state = marionette.emulator.battery.charging
        self.assertEquals(new_emulator_charging_state, (not emulator_charging))

        # verify that the 'chargingchange' listener was hit
        charging_changed = marionette.execute_async_script("""
        waitFor(function () {
            marionetteScriptFinished(global.chargingchanged);
        }, function () {
            return global.chargingchanged;
        });
        """, new_sandbox=False)
        self.assertTrue(charging_changed)

        # if we have set the charging state to 'off', set it back to 'on' to prevent
        # the emulator from sleeping
        if not new_emulator_charging_state:
            marionette.emulator.battery.charging = True

    def test_levelchange(self):
        marionette = self.marionette
        self.assertTrue(marionette.emulator.is_running)
        marionette.set_script_timeout(10000)

        # verify the emulator's battery status as reported by Gecko is the same as
        # reported by the device
        moz_level = marionette.execute_script("return navigator.mozBattery.level;",
                                              new_sandbox=False)
        self.assertEquals(moz_level, marionette.emulator.battery.level)

        # setup event listeners to be notified when the level or charging status 
        # changes
        self.assertTrue(marionette.execute_script("""
        global.levelchanged = false;
        navigator.mozBattery.addEventListener("levelchange", function() {
            global.levelchanged = true;
        });
        return true;
        """, new_sandbox=False))

        # set the battery to a new level, and verify
        if moz_level > 0.2:
            new_level = moz_level - 0.1
        else:
            new_level = moz_level + 0.1
        marionette.emulator.battery.level = new_level

        # XXX: do we need to wait here a bit?  this WFM...
        moz_level = marionette.emulator.battery.level
        self.assertEquals(int(new_level * 100), int(moz_level * 100))

        # verify that the 'levelchange' listener was hit
        level_changed = marionette.execute_async_script("""
        waitFor(function () {
            marionetteScriptFinished(global.levelchanged);
        }, function () {
            return global.levelchanged;
        });
        """, new_sandbox=False)
        self.assertTrue(level_changed)
