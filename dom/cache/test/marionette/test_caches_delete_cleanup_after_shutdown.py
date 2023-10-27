# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase

"""
 Currently we expect our size after cleanup to be 98k for the current
 database schema and page sizes and constants used in this test. We
 set the threshold at 128k so the test doesn't start failing if these
 control objects/structures increase somewhat in size, but should still
 fail if we fail to delete all of the 5,000 1k files we create on disk
 as part of the test.
"""
EXPECTED_CACHEDIR_SIZE_AFTER_CLEANUP = 128 * 1024  # 128KB
CACHE_ID = "data"


class CachesDeleteCleanupAtShutdownTestCase(MarionetteTestCase):

    """
    Bug1784700: This test ensures that cache body files get cleaned up
    properly after cache has been deleted. Note that body files gets
    cleaned up asynchronously which means that these files might still
    be around even after Caches.Delete promise gets resolved. Currently,
    we would only clean up body files on origin initialization and that's
    a firefox is necessary here.
    """

    def setUp(self):
        super(CachesDeleteCleanupAtShutdownTestCase, self).setUp()
        self.marionette.restart(in_app=False, clean=True)

    def tearDown(self):
        self.marionette.restart(in_app=False, clean=True)
        super(CachesDeleteCleanupAtShutdownTestCase, self).tearDown()

    def getUsage(self):
        return self.marionette.execute_script(
            """
                return window.wrappedJSObject.getStorageEstimate();
            """,
            new_sandbox=False,
        )

    def doCacheWork(self, n):
        # max timeout for this script to execute is 5 minutes
        maxTimeout = 5 * 60 * 1000

        return self.marionette.execute_script(
            """
                const [cacheId, n] = arguments;
                return window.wrappedJSObject.doCacheWork(cacheId, n);
            """,
            script_args=(
                CACHE_ID,
                n,
            ),
            new_sandbox=False,
            script_timeout=maxTimeout,
        )

    def openCache(self):
        return self.marionette.execute_async_script(
            """
                const [cacheId, resolve] = arguments;
                window.wrappedJSObject.openCache(cacheId).then(resolve("success"));
            """,
            new_sandbox=False,
            script_args=(CACHE_ID,),
        )

    def ensureCleanDirectory(self):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                """
                    let originDir = arguments[0];
                    const pathDelimiter = "/";

                    function getRelativeFile(relativePath) {
                        let file =  Services.dirsvc
                            .get("ProfD", Ci.nsIFile)
                            .clone();

                        relativePath.split(pathDelimiter).forEach(function(component) {
                            if (component == "..") {
                                file = file.parent;
                            } else {
                                file.append(component);
                            }
                        });

                        return file;
                    }

                    function getCacheDir() {

                        const storageDirName = "storage";
                        const defaultPersistenceDirName = "default";

                        return getRelativeFile(
                            `${storageDirName}/${defaultPersistenceDirName}/${originDir}/cache`
                        );
                    }

                    const cacheDir = getCacheDir();
                    let morgueDir = cacheDir.clone();

                    // morgue directory should be empty
                    // or atleast directories under morgue should be empty
                    morgueDir.append("morgue");
                    for (let dir of morgueDir.directoryEntries) {
                        for (let file of dir.directoryEntries) {
                            return false;
                        }
                    }
                    return true;
                """,
                script_args=(
                    self.marionette.absolute_url("")[:-1]
                    .replace(":", "+")
                    .replace("/", "+"),
                ),
                new_sandbox=False,
            )

    def create_and_cleanup_cache(self, ensureCleanCallback, in_app):
        # create 640 cache entries
        self.doCacheWork(640)

        self.marionette.restart(in_app=in_app)
        print("restart successful")

        self.marionette.navigate(
            self.marionette.absolute_url("dom/cache/cacheUsage.html")
        )
        return ensureCleanCallback()

    def test_ensure_cache_cleanup_after_clean_restart(self):
        self.marionette.navigate(
            self.marionette.absolute_url("dom/cache/cacheUsage.html")
        )
        beforeUsage = self.getUsage()

        def ensureCleanCallback():
            Wait(self.marionette, timeout=60).until(
                lambda x: (self.getUsage() - beforeUsage)
                < EXPECTED_CACHEDIR_SIZE_AFTER_CLEANUP,
                message="Cache directory is not cleaned up properly",
            )

            return (
                abs(beforeUsage - self.getUsage())
                <= EXPECTED_CACHEDIR_SIZE_AFTER_CLEANUP
                and self.ensureCleanDirectory()
            )

        if not self.create_and_cleanup_cache(ensureCleanCallback, True):
            print(f"beforeUsage = {beforeUsage}, and afterUsage = {self.getUsage()}")
            assert False

    def test_ensure_cache_cleanup_after_unclean_restart(self):
        self.marionette.navigate(
            self.marionette.absolute_url("dom/cache/cacheUsage.html")
        )
        beforeUsage = self.getUsage()

        def ensureCleanCallback():
            self.openCache()

            Wait(self.marionette, timeout=60).until(
                lambda x: (self.getUsage() - beforeUsage)
                < EXPECTED_CACHEDIR_SIZE_AFTER_CLEANUP,
                message="Cache directory is not cleaned up properly",
            )

            return (
                abs(beforeUsage - self.getUsage())
                <= EXPECTED_CACHEDIR_SIZE_AFTER_CLEANUP
                and self.ensureCleanDirectory()
            )

        if not self.create_and_cleanup_cache(ensureCleanCallback, False):
            print(f"beforeUsage = {beforeUsage}, and afterUsage = {self.getUsage()}")
            assert False
