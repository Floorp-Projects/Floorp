# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from external_media_harness.testcase import MediaTestCase, VideoPlaybackTestsMixin
from external_media_tests.media_utils.video_puppeteer import VideoException

reset_adobe_gmp_script = """
navigator.requestMediaKeySystemAccess('com.adobe.primetime',
[{initDataType: 'cenc'}]).then(
    function(access) {
        marionetteScriptFinished('success');
    },
    function(ex) {
        marionetteScriptFinished(ex);
    }
);
"""

class TestEMEPlayback(MediaTestCase, VideoPlaybackTestsMixin):

    # Class variable. We only need to reset the adobe GMP version once, not
    # every time we instantiate the class.
    version_needs_reset = True

    def setUp(self):
        super(TestEMEPlayback, self).setUp()
        self.set_eme_prefs()
        self.reset_GMP_version()
        assert(self.check_eme_prefs())

    def set_eme_prefs(self):
        with self.marionette.using_context('chrome'):

            # https://bugzilla.mozilla.org/show_bug.cgi?id=1187471#c28
            # 2015-09-28 cpearce says this is no longer necessary, but in case
            # we are working with older firefoxes...
            self.prefs.set_pref('media.gmp.trial-create.enabled', False)

    def reset_GMP_version(self):
        if TestEMEPlayback.version_needs_reset:
            with self.marionette.using_context('chrome'):
                if self.prefs.get_pref('media.gm-eme-adobe.version'):
                    self.prefs.set_pref('media.gm-eme-adobe.version', None)
                result = self.marionette.execute_async_script(reset_adobe_gmp_script,
                                                              script_timeout=60000)
                if not result == 'success':
                    raise VideoException('ERROR: Resetting Adobe GMP failed % s' % result)

            TestEMEPlayback.version_needs_reset = False

    def check_and_log_boolean_pref(self, pref_name, expected_value):
        with self.marionette.using_context('chrome'):
            pref_value = self.prefs.get_pref(pref_name)

            if pref_value is None:
                self.logger.info('Pref %s has no value.' % pref_name)
                return False
            else:
                self.logger.info('Pref %s = %s' % (pref_name, pref_value))
                if pref_value != expected_value:
                    self.logger.info('Pref %s has unexpected value.'
                                     % pref_name)
                    return False

        return True

    def check_and_log_integer_pref(self, pref_name, minimum_value=0):
        with self.marionette.using_context('chrome'):
            pref_value = self.prefs.get_pref(pref_name)

            if pref_value is None:
                self.logger.info('Pref %s has no value.' % pref_name)
                return False
            else:
                self.logger.info('Pref %s = %s' % (pref_name, pref_value))

                match = re.search('^\d+$', pref_value)
                if not match:
                    self.logger.info('Pref %s is not an integer' % pref_name)
                    return False

            return pref_value >= minimum_value

    def check_eme_prefs(self):
        with self.marionette.using_context('chrome'):
            prefs_ok = self.check_and_log_boolean_pref(
                'media.mediasource.enabled', True)
            prefs_ok = self.check_and_log_boolean_pref(
                'media.eme.enabled', True) and prefs_ok
            prefs_ok = self.check_and_log_boolean_pref(
                'media.mediasource.mp4.enabled', True) and prefs_ok
            prefs_ok = self.check_and_log_boolean_pref(
                'media.gmp-eme-adobe.enabled', True) and prefs_ok
            prefs_ok = self.check_and_log_integer_pref(
                'media.gmp-eme-adobe.version', 1) and prefs_ok

        return prefs_ok
