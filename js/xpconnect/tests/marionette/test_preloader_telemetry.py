# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import time
import os

from marionette_harness import MarionetteTestCase


LABELS_SCRIPT_PRELOADER_REQUESTS = {
    "Hit": "0",
    "HitChild": "1",
    "Miss": "2",
}


class TestScriptPreloader(MarionetteTestCase):
    def test_preloader_requests_histogram(self):
        start_time = time.time()
        self.invalidate_caches()

        self.marionette.restart(clean=False, in_app=True)

        histogram = self.get_histogram("SCRIPT_PRELOADER_REQUESTS")
        misses = histogram.get(LABELS_SCRIPT_PRELOADER_REQUESTS["Miss"], 0)
        hits = histogram.get(LABELS_SCRIPT_PRELOADER_REQUESTS["Hit"], 0)
        child_hits = histogram.get(LABELS_SCRIPT_PRELOADER_REQUESTS["HitChild"], 0)

        self.assertMuchGreaterThan(misses, hits)
        self.assertMuchGreaterThan(misses, child_hits)

        profile = self.marionette.profile_path
        self.wait_for_file_change(
            start_time, "{}/startupCache/scriptCache.bin".format(profile)
        )
        self.wait_for_file_change(
            start_time, "{}/startupCache/scriptCache-child.bin".format(profile)
        )
        self.marionette.restart(clean=False, in_app=True)

        histogram = self.get_histogram("SCRIPT_PRELOADER_REQUESTS")
        misses = histogram.get(LABELS_SCRIPT_PRELOADER_REQUESTS["Miss"], 0)
        hits = histogram.get(LABELS_SCRIPT_PRELOADER_REQUESTS["Hit"], 0)
        child_hits = histogram.get(LABELS_SCRIPT_PRELOADER_REQUESTS["HitChild"], 0)

        # This is just heuristic. We certainly want to be made aware if this ratio
        # changes and we didn't intend it to.
        self.assertSimilar(misses, hits)
        self.assertNotEqual(child_hits, 0)

    def assertMuchGreaterThan(self, lhs, rhs):
        self.assertGreater(lhs, 4 * rhs)

    def assertSimilar(self, lhs, rhs):
        self.assertLess(lhs, 2 * rhs)
        self.assertGreater(lhs, rhs / 2)

    def wait_for_file_change(self, start_time, path):
        expires = time.time() + 20
        while True:
            try:
                if os.stat(path).st_mtime > start_time:
                    return
            except OSError:
                pass
            if time.time() > expires:
                raise Exception("Never observed file change for {}".format(path))
            time.sleep(1)

    def wait_for_observer_notification(self, name):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_async_script(
                """
                const [name, resolve] = arguments;
                Services.obs.addObserver(() => resolve(), name);
            """,
                script_args=(name,),
            )

    def get_histogram(self, name):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(
                """
                const name = arguments[0];
                const snapshot = Services.telemetry.getSnapshotForHistograms("main", true);
                return snapshot.parent[name].values;
            """,
                script_args=(name,),
            )

    def invalidate_caches(self):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(
                "Services.obs.notifyObservers(null, 'startupcache-invalidate');"
            )
