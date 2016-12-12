# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import BrowserMobProxyTestCaseMixin

from external_media_harness.testcase import (
    EMESetupMixin,
    NetworkBandwidthTestCase,
    NetworkBandwidthTestsMixin,
)


class TestEMEPlaybackLimitingBandwidth(NetworkBandwidthTestCase,
                                       BrowserMobProxyTestCaseMixin,
                                       NetworkBandwidthTestsMixin,
                                       EMESetupMixin):


    def setUp(self):
        super(TestEMEPlaybackLimitingBandwidth, self).setUp()
        self.check_eme_system()

    # Tests in NetworkBandwidthTestsMixin
