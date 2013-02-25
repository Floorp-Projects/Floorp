# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os.path
import shutil
import zipfile
from StringIO import StringIO
import simplejson as json
import unittest
import cuddlefish
from cuddlefish import packaging, manifest

def up(path, generations=1):
    for i in range(generations):
        path = os.path.dirname(path)
    return path

ROOT = up(os.path.abspath(__file__), 4)
def get_linker_files_dir(name):
    return os.path.join(up(os.path.abspath(__file__)), "linker-files", name)

class Basic(unittest.TestCase):
    def get_pkg(self, name):
        d = get_linker_files_dir(name)
        return packaging.get_config_in_dir(d)

    def test_deps(self):
        target_cfg = self.get_pkg("one")
        pkg_cfg = packaging.build_config(ROOT, target_cfg)
        deps = packaging.get_deps_for_targets(pkg_cfg, ["one"])
        self.failUnlessEqual(deps, ["one"])
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        self.failUnlessEqual(deps, ["addon-sdk", "one"])

    def test_manifest(self):
        target_cfg = self.get_pkg("one")
        pkg_cfg = packaging.build_config(ROOT, target_cfg)
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        self.failUnlessEqual(deps, ["addon-sdk", "one"])
        # target_cfg.dependencies is not provided, so we'll search through
        # all known packages (everything in 'deps').
        m = manifest.build_manifest(target_cfg, pkg_cfg, deps, scan_tests=False)
        m = m.get_harness_options_manifest(False)

        def assertReqIs(modname, reqname, path):
            reqs = m["one/%s" % modname]["requirements"]
            self.failUnlessEqual(reqs[reqname], path)
        assertReqIs("main", "panel", "sdk/panel")
        assertReqIs("main", "two.js", "one/two")
        assertReqIs("main", "./two", "one/two")
        assertReqIs("main", "sdk/tabs.js", "sdk/tabs")
        assertReqIs("main", "./subdir/three", "one/subdir/three")
        assertReqIs("two", "main", "one/main")
        assertReqIs("subdir/three", "../main", "one/main")

        target_cfg.dependencies = []
        # now, because .dependencies *is* provided, we won't search 'deps',
        # so we'll get a link error
        self.assertRaises(manifest.ModuleNotFoundError,
                          manifest.build_manifest,
                          target_cfg, pkg_cfg, deps, scan_tests=False)

    def test_main_in_deps(self):
        target_cfg = self.get_pkg("three")
        package_path = [get_linker_files_dir("three-deps")]
        pkg_cfg = packaging.build_config(ROOT, target_cfg,
                                         packagepath=package_path)
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        self.failUnlessEqual(deps, ["addon-sdk", "three"])
        m = manifest.build_manifest(target_cfg, pkg_cfg, deps, scan_tests=False)
        m = m.get_harness_options_manifest(False)
        def assertReqIs(modname, reqname, path):
            reqs = m["three/%s" % modname]["requirements"]
            self.failUnlessEqual(reqs[reqname], path)
        assertReqIs("main", "three-a", "three-a/main")
        assertReqIs("main", "three-b", "three-b/main")
        assertReqIs("main", "three-c", "three-c/main")

    def test_relative_main_in_top(self):
        target_cfg = self.get_pkg("five")
        package_path = []
        pkg_cfg = packaging.build_config(ROOT, target_cfg,
                                         packagepath=package_path)
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        self.failUnlessEqual(deps, ["addon-sdk", "five"])
        # all we care about is that this next call doesn't raise an exception
        m = manifest.build_manifest(target_cfg, pkg_cfg, deps, scan_tests=False)
        m = m.get_harness_options_manifest(False)
        reqs = m["five/main"]["requirements"]
        self.failUnlessEqual(reqs, {});

    def test_unreachable_relative_main_in_top(self):
        target_cfg = self.get_pkg("six")
        package_path = []
        pkg_cfg = packaging.build_config(ROOT, target_cfg,
                                         packagepath=package_path)
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        self.failUnlessEqual(deps, ["addon-sdk", "six"])
        self.assertRaises(manifest.UnreachablePrefixError,
                          manifest.build_manifest,
                          target_cfg, pkg_cfg, deps, scan_tests=False)

    def test_unreachable_in_deps(self):
        target_cfg = self.get_pkg("four")
        package_path = [get_linker_files_dir("four-deps")]
        pkg_cfg = packaging.build_config(ROOT, target_cfg,
                                         packagepath=package_path)
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        self.failUnlessEqual(deps, ["addon-sdk", "four"])
        self.assertRaises(manifest.UnreachablePrefixError,
                          manifest.build_manifest,
                          target_cfg, pkg_cfg, deps, scan_tests=False)

class Contents(unittest.TestCase):

    def run_in_subdir(self, dirname, f, *args, **kwargs):
        top = os.path.abspath(os.getcwd())
        basedir = os.path.abspath(os.path.join(".test_tmp",self.id(),dirname))
        if os.path.isdir(basedir):
            assert basedir.startswith(top)
            shutil.rmtree(basedir)
        os.makedirs(basedir)
        try:
            os.chdir(basedir)
            return f(basedir, *args, **kwargs)
        finally:
            os.chdir(top)

    def assertIn(self, what, inside_what):
        self.failUnless(what in inside_what, inside_what)

    def test_jetpackID(self):
        # this uses "id": "jid7", to which a @jetpack should be appended
        seven = get_linker_files_dir("seven")
        def _test(basedir):
            stdout = StringIO()
            shutil.copytree(seven, "seven")
            os.chdir("seven")
            try:
                # regrettably, run() always finishes with sys.exit()
                cuddlefish.run(["xpi", "--no-strip-xpi"],
                               stdout=stdout)
            except SystemExit, e:
                self.failUnlessEqual(e.args[0], 0)
            zf = zipfile.ZipFile("seven.xpi", "r")
            hopts = json.loads(zf.read("harness-options.json"))
            self.failUnlessEqual(hopts["jetpackID"], "jid7@jetpack")
        self.run_in_subdir("x", _test)

    def test_jetpackID_suffix(self):
        # this uses "id": "jid1@jetpack", so no suffix should be appended
        one = get_linker_files_dir("one")
        def _test(basedir):
            stdout = StringIO()
            shutil.copytree(one, "one")
            os.chdir("one")
            try:
                # regrettably, run() always finishes with sys.exit()
                cuddlefish.run(["xpi", "--no-strip-xpi"],
                               stdout=stdout)
            except SystemExit, e:
                self.failUnlessEqual(e.args[0], 0)
            zf = zipfile.ZipFile("one.xpi", "r")
            hopts = json.loads(zf.read("harness-options.json"))
            self.failUnlessEqual(hopts["jetpackID"], "jid1@jetpack")
        self.run_in_subdir("x", _test)

    def test_strip_default(self):
        seven = get_linker_files_dir("seven")
        # now run 'cfx xpi' in that directory, except put the generated .xpi
        # elsewhere
        def _test(basedir):
            stdout = StringIO()
            shutil.copytree(seven, "seven")
            os.chdir("seven")
            try:
                # regrettably, run() always finishes with sys.exit()
                cuddlefish.run(["xpi"], # --strip-xpi is now the default
                               stdout=stdout)
            except SystemExit, e:
                self.failUnlessEqual(e.args[0], 0)
            zf = zipfile.ZipFile("seven.xpi", "r")
            names = zf.namelist()
            # the first problem found in bug 664840 was that cuddlefish.js
            # (the loader) was stripped out on windows, due to a /-vs-\ bug
            self.assertIn("resources/addon-sdk/lib/sdk/loader/cuddlefish.js", names)
            # the second problem found in bug 664840 was that an addon
            # without an explicit tests/ directory would copy all files from
            # the package into a bogus JID-PKGNAME-tests/ directory, so check
            # for that
            testfiles = [fn for fn in names if "seven/tests" in fn]
            self.failUnlessEqual([], testfiles)
            # the third problem was that data files were being stripped from
            # the XPI. Note that data/ is only supposed to be included if a
            # module that actually gets used does a require("self") .
            self.assertIn("resources/seven/data/text.data",
                          names)
            self.failIf("seven/lib/unused.js"
                        in names, names)
        self.run_in_subdir("x", _test)

    def test_no_strip(self):
        seven = get_linker_files_dir("seven")
        def _test(basedir):
            stdout = StringIO()
            shutil.copytree(seven, "seven")
            os.chdir("seven")
            try:
                # regrettably, run() always finishes with sys.exit()
                cuddlefish.run(["xpi", "--no-strip-xpi"],
                               stdout=stdout)
            except SystemExit, e:
                self.failUnlessEqual(e.args[0], 0)
            zf = zipfile.ZipFile("seven.xpi", "r")
            names = zf.namelist()
            self.assertIn("resources/addon-sdk/lib/sdk/loader/cuddlefish.js", names)
            testfiles = [fn for fn in names if "seven/tests" in fn]
            self.failUnlessEqual([], testfiles)
            self.assertIn("resources/seven/data/text.data",
                          names)
            self.failUnless("resources/seven/lib/unused.js"
                            in names, names)
        self.run_in_subdir("x", _test)


if __name__ == '__main__':
    unittest.main()
