# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os
from mozunit import main, MockedOpen
import unittest
from tempfile import mkstemp


class TestMozUnit(unittest.TestCase):
    def test_mocked_open(self):
        # Create a temporary file on the file system.
        (fd, path) = mkstemp()
        with os.fdopen(fd, "w") as file:
            file.write("foobar")

        self.assertFalse(os.path.exists("file1"))
        self.assertFalse(os.path.exists("file2"))

        with MockedOpen({"file1": "content1", "file2": "content2"}):
            self.assertTrue(os.path.exists("file1"))
            self.assertTrue(os.path.exists("file2"))
            self.assertFalse(os.path.exists("foo/file1"))

            # Check the contents of the files given at MockedOpen creation.
            self.assertEqual(open("file1", "r").read(), "content1")
            self.assertEqual(open("file2", "r").read(), "content2")

            # Check that overwriting these files alters their content.
            with open("file1", "w") as file:
                file.write("foo")
            self.assertTrue(os.path.exists("file1"))
            self.assertEqual(open("file1", "r").read(), "foo")

            # ... but not until the file is closed.
            file = open("file2", "w")
            file.write("bar")
            self.assertEqual(open("file2", "r").read(), "content2")
            file.close()
            self.assertEqual(open("file2", "r").read(), "bar")

            # Check that appending to a file does append
            with open("file1", "a") as file:
                file.write("bar")
            self.assertEqual(open("file1", "r").read(), "foobar")

            self.assertFalse(os.path.exists("file3"))

            # Opening a non-existing file ought to fail.
            self.assertRaises(IOError, open, "file3", "r")
            self.assertFalse(os.path.exists("file3"))

            # Check that writing a new file does create the file.
            with open("file3", "w") as file:
                file.write("baz")
            self.assertEqual(open("file3", "r").read(), "baz")
            self.assertTrue(os.path.exists("file3"))

            # Check the content of the file created outside MockedOpen.
            self.assertEqual(open(path, "r").read(), "foobar")

            # Check that overwriting a file existing on the file system
            # does modify its content.
            with open(path, "w") as file:
                file.write("bazqux")
            self.assertEqual(open(path, "r").read(), "bazqux")

        with MockedOpen():
            # Check that appending to a file existing on the file system
            # does modify its content.
            with open(path, "a") as file:
                file.write("bazqux")
            self.assertEqual(open(path, "r").read(), "foobarbazqux")

        # Check that the file was not actually modified on the file system.
        self.assertEqual(open(path, "r").read(), "foobar")
        os.remove(path)

        # Check that the file created inside MockedOpen wasn't actually
        # created.
        self.assertRaises(IOError, open, "file3", "r")


if __name__ == "__main__":
    main()
