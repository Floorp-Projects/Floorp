
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import os.path

parent = os.path.dirname
test_dir = parent(os.path.abspath(__file__))
sdk_root = parent(parent(parent(test_dir)))

def from_sdk_top(fn):
    return os.path.abspath(os.path.join(sdk_root, fn))

MPL2_URL = "http://mozilla.org/MPL/2.0/"

# These files all come with their own license headers
skip = [
    "python-lib/cuddlefish/_version.py", # generated, public domain
    "doc/static-files/js/jquery.js", # MIT/GPL dual
    "examples/annotator/data/jquery-1.4.2.min.js", # MIT/GPL dual
    "examples/reddit-panel/data/jquery-1.4.4.min.js", # MIT/GPL dual
    "examples/library-detector/data/library-detector.js", # MIT
    "python-lib/mozrunner/killableprocess.py", # MIT? BSDish?
    "python-lib/mozrunner/winprocess.py", # MIT
    "packages/api-utils/tests/test-querystring.js", # MIT
    "packages/api-utils/lib/promise.js", # MIT
    "packages/api-utils/tests/test-promise.js", # MIT
    "examples/actor-repl/README.md", # It's damn readme file
    "examples/actor-repl/data/codemirror-compressed.js", # MIT
    "examples/actor-repl/data/codemirror.css", # MIT
    ]
absskip = [from_sdk_top(os.path.join(*fn.split("/"))) for fn in skip]

class Licenses(unittest.TestCase):
    def test(self):
        # Examine most SDK files to check if they've got an MPL2 license
        # header. We exclude some files that are known to include different
        # licenses.
        self.missing = []
        self.scan_file(from_sdk_top(os.path.join("python-lib", "jetpack_sdk_env.py")))
        self.scan(os.path.join("python-lib", "cuddlefish"), [".js", ".py"],
                  skipdirs=["sdk-docs"], # test_generate.py makes this
                  )
        self.scan(os.path.join("python-lib", "mozrunner"), [".py"])
        self.scan("lib", [".js", ".jsm", ".css"],
                  skipdirs=[
                    "diffpatcher", # MIT
                    "method", # MIT
                    "child_process", # MPL 1.1/GPL 2.0/LGPL 2.1
                    "fs" # MIT
                  ])
        self.scan("test", [".js", ".jsm", ".css", ".html"],
                  skipdirs=[
                    "buffers", # MIT
                    "querystring", # MIT
                    "path" # MIT
                  ])
        self.scan("modules", [".js", ".jsm"])
        self.scan("examples", [".js", ".css", ".html", ".md"])
        self.scan("bin", [".bat", ".ps1", ".js"])
        for fn in [os.path.join("bin", "activate"),
                   os.path.join("bin", "cfx"),
                   os.path.join("bin", "integration-scripts", "buildbot-run-cfx-helper"),
                   os.path.join("bin", "integration-scripts", "integration-check"),
                   ]:
            self.scan_file(from_sdk_top(fn))

        if self.missing:
            print
            print "The following files are missing an MPL2 header:"
            for fn in sorted(self.missing):
                print " "+fn
            self.fail("%d files are missing an MPL2 header" % len(self.missing))

    def scan(self, start, extensions=[], skipdirs=[]):
        # scan a whole subdirectory
        start = from_sdk_top(start)
        for root, dirs, files in os.walk(start):
            for d in skipdirs:
                if d in dirs:
                    dirs.remove(d)
            for fn in files:
                ext = os.path.splitext(fn)[1]
                if extensions and ext not in extensions:
                    continue
                absfn = os.path.join(root, fn)
                if absfn in absskip:
                    continue
                self.scan_file(absfn)

    def scan_file(self, fn):
        # scan a single file
        if not MPL2_URL in open(fn, "r").read():
            relfile = fn[len(sdk_root)+1:]
            self.missing.append(relfile)

if __name__ == '__main__':
    unittest.main()
