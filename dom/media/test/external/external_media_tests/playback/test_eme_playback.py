# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from external_media_harness.testcase import MediaTestCase, VideoPlaybackTestsMixin, EMESetupMixin


class TestEMEPlayback(MediaTestCase, VideoPlaybackTestsMixin, EMESetupMixin):

    def setUp(self):
        super(TestEMEPlayback, self).setUp()
        self.check_eme_system()

    # Tests are implemented in VideoPlaybackTestsMixin
