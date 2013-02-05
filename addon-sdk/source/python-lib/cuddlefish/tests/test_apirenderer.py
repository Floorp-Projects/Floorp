# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import unittest
from cuddlefish.docs.apirenderer import md_to_html

tests_path = os.path.abspath(os.path.dirname(__file__))
static_files_path = os.path.join(tests_path, "static-files")

class ParserTests(unittest.TestCase):
    def pathname(self, filename):
        return os.path.join(static_files_path, "docs", filename)

    def render_markdown(self, pathname):
        return md_to_html(pathname, "APIsample")

    def test_renderer(self):
        test = self.render_markdown(self.pathname("APIsample.md"))
        reference = open(self.pathname("APIreference.html")).read()
        test_lines = test.splitlines(True)
        reference_lines = reference.splitlines(True)
        for x in range(len(test_lines)):
            self.assertEqual(test_lines[x], reference_lines[x],
                             "line %d: expected '%s', got '%s'"
                             % (x+1, reference_lines[x], test_lines[x]))

if __name__ == "__main__":
    unittest.main()
