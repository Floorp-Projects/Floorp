# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait
from marionette_harness.marionette_test import MarionetteTestCase


class TestPingSubmitted(MarionetteTestCase):
    def setUp(self):
        super(TestPingSubmitted, self).setUp()

        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        self.marionette.enforce_gecko_prefs(
            {
                "browser.search.serpEventTelemetryCategorization.enabled": True,
                "datareporting.healthreport.uploadEnabled": True,
                "telemetry.fog.test.localhost_port": 3000,
                "browser.search.log": True,
            }
        )

    def test_ping_submit_on_start(self):
        # Record an event for the ping to eventually submit.
        self.marionette.execute_script(
            """
        Glean.serp.categorization.record({
            organic_category: "3",
            organic_num_domains: "1",
            organic_num_inconclusive: "0",
            organic_num_unknown: "0",
            sponsored_category: "4",
            sponsored_num_domains: "2",
            sponsored_num_inconclusive: "0",
            sponsored_num_unknown: "0",
            mappings_version: "1",
            app_version: "124",
            channel: "nightly",
            region: "US",
            partner_code: "ff",
            provider: "example",
            tagged: "true",
            num_ads_clicked: "0",
            num_ads_visible: "2",
        });
            """
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
            return Glean.serp.categorization.testGetValue()?.length ?? 0;
                """
            ),
            1,
            msg="Should have recorded a SERP categorization event before restart.",
        )

        self.marionette.restart(clean=False, in_app=True)

        Wait(self.marionette, timeout=60).until(
            lambda _: self.marionette.execute_script(
                """
            return (Glean.serp.categorization.testGetValue()?.length ?? 0) == 0;
                """
            ),
            0,
            message="SERP categorization should have been sent some time after restart.",
        )
