# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import BrowserMobProxyTestCaseMixin

from external_media_harness.testcase import (
    NetworkBandwidthTestCase, NetworkBandwidthTestsMixin
)


class TestPlaybackLimitingBandwidth(NetworkBandwidthTestCase,
                                    NetworkBandwidthTestsMixin,
                                    BrowserMobProxyTestCaseMixin):

    # Tests are in NetworkBandwidthTestsMixin
    pass
