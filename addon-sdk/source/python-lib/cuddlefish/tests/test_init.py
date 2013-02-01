# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, unittest, shutil
from StringIO import StringIO
from cuddlefish import initializer
from cuddlefish.templates import TEST_MAIN_JS, PACKAGE_JSON

tests_path = os.path.abspath(os.path.dirname(__file__))

class TestInit(unittest.TestCase):

    def run_init_in_subdir(self, dirname, f, *args, **kwargs):
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

    def do_test_init(self,basedir):
        # Let's init the addon, no error admitted
        f = open(".ignoreme","w")
        f.write("stuff")
        f.close()

        out, err = StringIO(), StringIO()
        init_run = initializer(None, ["init"], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.assertEqual(init_run["result"], 0)
        self.assertTrue("* lib directory created" in out)
        self.assertTrue("* data directory created" in out)
        self.assertTrue("Have fun!" in out)
        self.assertEqual(err,"")
        self.assertTrue(len(os.listdir(basedir))>0)
        main_js = os.path.join(basedir,"lib","main.js")
        package_json = os.path.join(basedir,"package.json")
        test_main_js = os.path.join(basedir,"test","test-main.js")
        self.assertTrue(os.path.exists(main_js))
        self.assertTrue(os.path.exists(package_json))
        self.assertTrue(os.path.exists(test_main_js))
        self.assertEqual(open(main_js,"r").read(),"")
        self.assertEqual(open(package_json,"r").read() % {"id":"tmp_addon_id" },
                         PACKAGE_JSON % {"name":"tmp_addon_sample",
                                         "fullName": "tmp_addon_SAMPLE",
                                         "id":init_run["jid"] })
        self.assertEqual(open(test_main_js,"r").read(),TEST_MAIN_JS)

        # Let's check that the addon is initialized
        out, err = StringIO(), StringIO()
        init_run = initializer(None, ["init"], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.failIfEqual(init_run["result"],0)
        self.assertTrue("This command must be run in an empty directory." in err)

    def test_initializer(self):
        self.run_init_in_subdir("tmp_addon_SAMPLE",self.do_test_init)

    def do_test_args(self, basedir):
        # check that running it with spurious arguments will fail
        out,err = StringIO(), StringIO()
        init_run = initializer(None, ["init", "specified-dirname", "extra-arg"], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.failIfEqual(init_run["result"], 0)
        self.assertTrue("Too many arguments" in err)

    def test_args(self):
        self.run_init_in_subdir("tmp_addon_sample", self.do_test_args)

    def _test_existing_files(self, basedir):
        f = open("pay_attention_to_me","w")
        f.write("stuff")
        f.close()
        out,err = StringIO(), StringIO()
        rc = initializer(None, ["init"], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.assertEqual(rc["result"], 1)
        self.failUnless("This command must be run in an empty directory" in err,
                        err)
        self.failIf(os.path.exists("lib"))

    def test_existing_files(self):
        self.run_init_in_subdir("existing_files", self._test_existing_files)

    def test_init_subdir(self):
        parent = os.path.abspath(os.path.join(".test_tmp", self.id()))
        basedir = os.path.join(parent, "init-basedir")
        if os.path.exists(parent):
            shutil.rmtree(parent)
        os.makedirs(parent)

        # if the basedir exists and is not empty, init should refuse
        os.makedirs(basedir)
        f = open(os.path.join(basedir, "boo"), "w")
        f.write("stuff")
        f.close()
        out, err = StringIO(), StringIO()
        rc = initializer(None, ["init", basedir], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.assertEqual(rc["result"], 1)
        self.assertTrue("testing if directory is empty" in out, out)
        self.assertTrue("This command must be run in an empty directory." in err,
                        err)

        # a .dotfile should be tolerated
        os.rename(os.path.join(basedir, "boo"), os.path.join(basedir, ".phew"))
        out, err = StringIO(), StringIO()
        rc = initializer(None, ["init", basedir], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.assertEqual(rc["result"], 0)
        self.assertTrue("* data directory created" in out, out)
        self.assertTrue("Have fun!" in out)
        self.assertEqual(err,"")
        self.assertTrue(os.listdir(basedir))
        main_js = os.path.join(basedir,"lib","main.js")
        package_json = os.path.join(basedir,"package.json")
        self.assertTrue(os.path.exists(main_js))
        self.assertTrue(os.path.exists(package_json))
        shutil.rmtree(basedir)

        # init should create directories that don't exist already
        out, err = StringIO(), StringIO()
        rc = initializer(None, ["init", basedir], out, err)
        out, err = out.getvalue(), err.getvalue()
        self.assertEqual(rc["result"], 0)
        self.assertTrue("* data directory created" in out)
        self.assertTrue("Have fun!" in out)
        self.assertEqual(err,"")
        self.assertTrue(os.listdir(basedir))
        main_js = os.path.join(basedir,"lib","main.js")
        package_json = os.path.join(basedir,"package.json")
        self.assertTrue(os.path.exists(main_js))
        self.assertTrue(os.path.exists(package_json))


class TestCfxQuits(unittest.TestCase):

    def run_cfx(self, addon_name, command):
        old_cwd = os.getcwd()
        addon_path = os.path.join(tests_path,
                                  "addons", addon_name)
        os.chdir(addon_path)
        import sys
        old_stdout = sys.stdout
        old_stderr = sys.stderr
        sys.stdout = out = StringIO()
        sys.stderr = err = StringIO()
        try:
            import cuddlefish
            args = list(command)
            # Pass arguments given to cfx so that cfx can find firefox path
            # if --binary option is given:
            args.extend(sys.argv[1:])
            cuddlefish.run(arguments=args)
        except SystemExit, e:
            if "code" in e:
                rc = e.code
            elif "args" in e and len(e.args)>0:
                rc = e.args[0]
            else:
                rc = 0
        finally:
            sys.stdout = old_stdout
            sys.stderr = old_stderr
            os.chdir(old_cwd)
        out.flush()
        err.flush()
        return rc, out.getvalue(), err.getvalue()

    # this method doesn't exists in python 2.5,
    # implements our own
    def assertIn(self, member, container):
        """Just like self.assertTrue(a in b), but with a nicer default message."""
        if member not in container:
            standardMsg = '"%s" not found in "%s"' % (member,
                                                  container)
            self.fail(standardMsg)

    def test_run(self):
        rc, out, err = self.run_cfx("simplest-test", ["run"])
        self.assertEqual(rc, 0)
        self.assertIn("Program terminated successfully.", err)

    def test_test(self):
        rc, out, err = self.run_cfx("simplest-test", ["test"])
        self.assertEqual(rc, 0)
        self.assertIn("1 of 1 tests passed.", err)
        self.assertIn("Program terminated successfully.", err)


if __name__ == "__main__":
    unittest.main()
