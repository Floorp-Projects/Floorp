# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
import unittest

from mozunit import main

from mozbuild.base import MozbuildObject
from mozpack.files import FileFinder
from mozbuild.frontend.context import Files
from mozbuild.frontend.reader import (
    BuildReader,
    EmptyConfig,
)


class TestMozbuildReading(unittest.TestCase):
    # This hack is needed to appease running in automation.
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop('MOZCONFIG', None)
        os.environ.pop('MOZ_OBJDIR', None)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

    def _mozbuilds(self, reader):
        if not hasattr(self, '_mozbuild_paths'):
            self._mozbuild_paths = set(reader.all_mozbuild_paths())

        return self._mozbuild_paths

    @unittest.skip('failing in SpiderMonkey builds')
    def test_filesystem_traversal_reading(self):
        """Reading moz.build according to filesystem traversal works.

        We attempt to read every known moz.build file via filesystem traversal.

        If this test fails, it means that metadata extraction will fail.
        """
        mb = MozbuildObject.from_environment(detect_virtualenv_mozinfo=False)
        config = mb.config_environment
        reader = BuildReader(config)
        all_paths = self._mozbuilds(reader)
        paths, contexts = reader.read_relevant_mozbuilds(all_paths)
        self.assertEqual(set(paths), all_paths)
        self.assertGreaterEqual(len(contexts), len(paths))

    def test_filesystem_traversal_no_config(self):
        """Reading moz.build files via filesystem traversal mode with no build config.

        This is similar to the above test except no build config is applied.
        This will likely fail in more scenarios than the above test because a
        lot of moz.build files assumes certain variables are present.
        """
        here = os.path.abspath(os.path.dirname(__file__))
        root = os.path.normpath(os.path.join(here, '..', '..'))
        config = EmptyConfig(root)
        reader = BuildReader(config)
        all_paths = self._mozbuilds(reader)
        paths, contexts = reader.read_relevant_mozbuilds(all_paths)
        self.assertEqual(set(paths.keys()), all_paths)
        self.assertGreaterEqual(len(contexts), len(paths))

    def test_orphan_file_patterns(self):
        if sys.platform == 'win32':
            raise unittest.SkipTest('failing on windows builds')

        mb = MozbuildObject.from_environment(detect_virtualenv_mozinfo=False)

        try:
            config = mb.config_environment
        except Exception as e:
            if e.message == 'config.status not available. Run configure.':
                raise unittest.SkipTest('failing without config.status')
            raise

        if config.substs['MOZ_BUILD_APP'] == 'js':
            raise unittest.SkipTest('failing in Spidermonkey builds')

        reader = BuildReader(config)
        all_paths = self._mozbuilds(reader)
        _, contexts = reader.read_relevant_mozbuilds(all_paths)

        finder = FileFinder(config.topsrcdir, ignore=['obj*'])

        def pattern_exists(pat):
            return [p for p in finder.find(pat)] != []

        for ctx in contexts:
            if not isinstance(ctx, Files):
                continue
            relsrcdir = ctx.relsrcdir
            if not pattern_exists(os.path.join(relsrcdir, ctx.pattern)):
                self.fail("The pattern '%s' in a Files() entry in "
                          "'%s' corresponds to no files in the tree.\n"
                          "Please update this entry." %
                          (ctx.pattern, ctx.main_path))
            test_files = ctx['IMPACTED_TESTS'].files
            for p in test_files:
                if not pattern_exists(os.path.relpath(p.full_path, config.topsrcdir)):
                    self.fail("The pattern '%s' in a dependent tests entry "
                              "in '%s' corresponds to no files in the tree.\n"
                              "Please update this entry." %
                              (p, ctx.main_path))


if __name__ == '__main__':
    main()
