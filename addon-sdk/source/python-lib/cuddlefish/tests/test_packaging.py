# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest

from cuddlefish import packaging
from cuddlefish.bunch import Bunch

tests_path = os.path.abspath(os.path.dirname(__file__))
static_files_path = os.path.join(tests_path, 'static-files')

def get_configs(pkg_name, dirname='static-files'):
    root_path = os.path.join(tests_path, dirname)
    pkg_path = os.path.join(root_path, 'packages', pkg_name)
    if not (os.path.exists(pkg_path) and os.path.isdir(pkg_path)):
        raise Exception('path does not exist: %s' % pkg_path)
    target_cfg = packaging.get_config_in_dir(pkg_path)
    pkg_cfg = packaging.build_config(root_path, target_cfg)
    deps = packaging.get_deps_for_targets(pkg_cfg, [pkg_name])
    build = packaging.generate_build_for_target(
        pkg_cfg=pkg_cfg,
        target=pkg_name,
        deps=deps
        )
    return Bunch(target_cfg=target_cfg, pkg_cfg=pkg_cfg, build=build)

class PackagingTests(unittest.TestCase):
    def test_bug_588661(self):
        configs = get_configs('foo', 'bug-588661-files')
        self.assertEqual(configs.build.loader,
                         'foo/lib/foo-loader.js')

    def test_bug_614712(self):
        configs = get_configs('commonjs-naming', 'bug-614712-files')
        packages = configs.pkg_cfg.packages
        base = os.path.join(tests_path, 'bug-614712-files', 'packages')
        self.assertEqual(packages['original-naming'].tests,
                         [os.path.join(base, 'original-naming', 'tests')])
        self.assertEqual(packages['commonjs-naming'].tests,
                         [os.path.join(base, 'commonjs-naming', 'test')])

    def test_basic(self):
        configs = get_configs('aardvark')
        packages = configs.pkg_cfg.packages

        self.assertTrue('addon-sdk' in packages)
        self.assertTrue('aardvark' in packages)
        self.assertTrue('addon-sdk' in packages.aardvark.dependencies)
        self.assertEqual(packages['addon-sdk'].loader, 'lib/sdk/loader/cuddlefish.js')
        self.assertTrue(packages.aardvark.main == 'main')
        self.assertTrue(packages.aardvark.version == "1.0")

class PackagePath(unittest.TestCase):
    def test_packagepath(self):
        root_path = os.path.join(tests_path, 'static-files')
        pkg_path = os.path.join(root_path, 'packages', 'minimal')
        target_cfg = packaging.get_config_in_dir(pkg_path)
        pkg_cfg = packaging.build_config(root_path, target_cfg)
        base_packages = set(pkg_cfg.packages.keys())
        ppath = [os.path.join(tests_path, 'bug-611495-files')]
        pkg_cfg2 = packaging.build_config(root_path, target_cfg, packagepath=ppath)
        all_packages = set(pkg_cfg2.packages.keys())
        self.assertEqual(sorted(["jspath-one"]),
                         sorted(all_packages - base_packages))

class Directories(unittest.TestCase):
    # for bug 652227
    packages_path = os.path.join(tests_path, "bug-652227-files", "packages")
    def get_config(self, pkg_name):
        pkg_path = os.path.join(tests_path, "bug-652227-files", "packages",
                                pkg_name)
        return packaging.get_config_in_dir(pkg_path)

    def test_explicit_lib(self):
        # package.json provides .lib
        p = self.get_config('explicit-lib')
        self.assertEqual(os.path.abspath(p.lib[0]),
                         os.path.abspath(os.path.join(self.packages_path,
                                                      "explicit-lib",
                                                      "alt2-lib")))

    def test_directories_lib(self):
        # package.json provides .directories.lib
        p = self.get_config('explicit-dir-lib')
        self.assertEqual(os.path.abspath(p.lib[0]),
                         os.path.abspath(os.path.join(self.packages_path,
                                                      "explicit-dir-lib",
                                                      "alt-lib")))

    def test_lib(self):
        # package.json is empty, but lib/ exists
        p = self.get_config("default-lib")
        self.assertEqual(os.path.abspath(p.lib[0]),
                         os.path.abspath(os.path.join(self.packages_path,
                                                      "default-lib",
                                                      "lib")))

    def test_root(self):
        # package.json is empty, no lib/, so files are in root
        p = self.get_config('default-root')
        self.assertEqual(os.path.abspath(p.lib[0]),
                         os.path.abspath(os.path.join(self.packages_path,
                                                      "default-root")))

    def test_locale(self):
        # package.json is empty, but locale/ exists and should be used
        p = self.get_config("default-locale")
        self.assertEqual(os.path.abspath(p.locale),
                         os.path.abspath(os.path.join(self.packages_path,
                                                      "default-locale",
                                                      "locale")))

if __name__ == "__main__":
    unittest.main()
