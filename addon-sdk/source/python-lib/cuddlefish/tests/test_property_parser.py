# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from cuddlefish.property_parser import parse, MalformedLocaleFileError

class TestParser(unittest.TestCase):

    def test_parse(self):
        lines = [
          # Comments are striped only if `#` is the first non-space character
          "sharp=#can be in value",
          "# comment",
          "#key=value",
          "  # comment2",

          "keyWithNoValue=",
          "valueWithSpaces=   ",
          "valueWithMultilineSpaces=  \\",
          "  \\",
          "  ",

          # All spaces before/after are striped
          " key = value ",
          "key2=value2",
          # Keys can contain '%'
          "%s key=%s value",

          # Accept empty lines
          "",
          "   ",

          # Multiline string must use backslash at end of lines
          "multi=line\\", "value",
          # With multiline string, left spaces are stripped ...
          "some= spaces\\", " are\\  ", " stripped ",
          # ... but not right spaces, except the last line!
          "but=not \\", "all of \\", " them ",

          # Explicit [other] plural definition
          "explicitPlural[one] = one",
          "explicitPlural[other] = other",

          # Implicit [other] plural definition
          "implicitPlural[one] = one",
          "implicitPlural = other", # This key is the [other] one
        ]
        # Ensure that all lines end with a `\n`
        # And that strings are unicode ones (parser code relies on it)
        lines = [unicode(l + "\n") for l in lines]
        pairs = parse(lines)
        expected = {
          "sharp": "#can be in value",

          "key": "value",
          "key2": "value2",
          "%s key": "%s value",

          "keyWithNoValue": "",
          "valueWithSpaces": "",
          "valueWithMultilineSpaces": "",

          "multi": "linevalue",
          "some": "spacesarestripped",
          "but": "not all of them",

          "implicitPlural": {
            "one": "one",
            "other": "other"
          },
          "explicitPlural": {
            "one": "one",
            "other": "other"
          },
        }
        self.assertEqual(pairs, expected)

    def test_exceptions(self):
        self.failUnlessRaises(MalformedLocaleFileError, parse,
                              ["invalid line with no key value"])
        self.failUnlessRaises(MalformedLocaleFileError, parse,
                              ["plural[one]=plural with no [other] value"])
        self.failUnlessRaises(MalformedLocaleFileError, parse,
                              ["multiline with no last empty line=\\"])
        self.failUnlessRaises(MalformedLocaleFileError, parse,
                              ["=no key"])
        self.failUnlessRaises(MalformedLocaleFileError, parse,
                              ["   =only spaces in key"])

if __name__ == "__main__":
    unittest.main()
