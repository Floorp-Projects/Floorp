# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import xml.dom.minidom
import os.path

from cuddlefish import rdf, packaging, property_parser

parent = os.path.dirname
test_dir = parent(os.path.abspath(__file__))
template_dir = os.path.join(parent(test_dir), "../../app-extension")

class RDFTests(unittest.TestCase):
    def testBug567660(self):
        obj = rdf.RDF()
        data = u'\u2026'.encode('utf-8')
        x = '<?xml version="1.0" encoding="utf-8"?><blah>%s</blah>' % data
        obj.dom = xml.dom.minidom.parseString(x)
        self.assertEqual(obj.dom.documentElement.firstChild.nodeValue,
                         u'\u2026')
        self.assertEqual(str(obj).replace("\n",""), x.replace("\n",""))

    def failUnlessIn(self, substring, s, msg=""):
        if substring not in s:
            self.fail("(%s) substring '%s' not in string '%s'"
                      % (msg, substring, s))

    def testUnpack(self):
        basedir = os.path.join(test_dir, "bug-715340-files")
        for n in ["pkg-1-pack", "pkg-2-unpack", "pkg-3-pack"]:
            cfg = packaging.get_config_in_dir(os.path.join(basedir, n))
            m = rdf.gen_manifest(template_dir, cfg, jid="JID")
            if n.endswith("-pack"):
                # these ones should remain packed
                self.failUnlessEqual(m.get("em:unpack"), "false")
                self.failUnlessIn("<em:unpack>false</em:unpack>", str(m), n)
            else:
                # and these should be unpacked
                self.failUnlessEqual(m.get("em:unpack"), "true")
                self.failUnlessIn("<em:unpack>true</em:unpack>", str(m), n)

    def testTitle(self):
        basedir = os.path.join(test_dir, 'bug-906359-files')
        for n in ['title', 'fullName', 'none']:
            cfg = packaging.get_config_in_dir(os.path.join(basedir, n))
            m = rdf.gen_manifest(template_dir, cfg, jid='JID')
            self.failUnlessEqual(m.get('em:name'), 'a long ' + n)
            self.failUnlessIn('<em:name>a long ' + n + '</em:name>', str(m), n)

    def testLocalization(self):
        # addon_title       -> <em:name>
        # addon_author      -> <em:creator>
        # addon_description -> <em:description>
        # addon_homepageURL -> <em:homepageURL>
        localizable_in = ["title", "author", "description", "homepage"]
        localized_out  = ["name", "creator", "description", "homepageURL"]

        basedir = os.path.join(test_dir, "bug-661083-files/packages")
        for n in ["noLocalization", "twoLanguages"]:
            harness_options = { "locale" : {} }
            pkgdir = os.path.join(basedir, n)
            localedir = os.path.join(pkgdir, "locale")
            files = os.listdir(localedir)

            for file in files:
                filepath = os.path.join(localedir, file)
                if os.path.isfile(filepath) and file.endswith(".properties"):
                    language = file[:-len(".properties")]
                    try:
                        parsed_file = property_parser.parse_file(filepath)
                    except property_parser.MalformedLocaleFileError, msg:
                        self.fail(msg)

                    harness_options["locale"][language] = parsed_file

            cfg = packaging.get_config_in_dir(pkgdir)
            m = rdf.gen_manifest(template_dir, cfg, 'JID', harness_options)

            if n == "noLocalization":
                self.failIf("<em:locale>" in str(m))
                continue

            for lang in harness_options["locale"]:
                rdfstr = str(m)
                node = "<em:locale>" + lang + "</em:locale>"
                self.failUnlessIn(node, rdfstr, n)

                for value_in in localizable_in:
                    key_in = "extensions." + m.get('em:id') + "." + value_in
                    tag_out = localized_out[localizable_in.index(value_in)]
                    if key_in in harness_options["locale"][lang]:
                        # E.g. "<em:creator>author-en-US</em:creator>"
                        node = "<em:" + tag_out + ">" + value_in + "-" + lang \
                            + "</em:" + tag_out + ">"
                        self.failUnlessIn(node , rdfstr, n)

if __name__ == '__main__':
    unittest.main()
