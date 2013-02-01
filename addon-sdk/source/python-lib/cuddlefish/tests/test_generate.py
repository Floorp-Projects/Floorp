# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import unittest
import StringIO
import tarfile
import HTMLParser
import urlparse
import urllib

from cuddlefish.docs import generate
from cuddlefish.tests import env_root

INITIAL_FILESET = [ ["static-files", "base.html"], \
                    ["dev-guide", "index.html"], \
                    ["modules", "sdk", "aardvark-feeder.html"], \
                    ["modules", "sdk", "anteater", "anteater.html"], \
                    ["modules", "packages", "third_party.html"]]

EXTENDED_FILESET = [ ["static-files", "base.html"], \
                    ["dev-guide", "extra.html"], \
                    ["dev-guide", "index.html"], \
                    ["modules", "sdk", "aardvark-feeder.html"], \
                    ["modules", "sdk", "anteater", "anteater.html"],\
                    ["modules", "packages", "third_party.html"]]

EXTRAFILE = ["dev-guide", "extra.html"]

def get_test_root():
    return os.path.join(env_root, "python-lib", "cuddlefish", "tests", "static-files")

def get_sdk_docs_root():
    return os.path.join(get_test_root(), "sdk-docs")

def get_base_url_path():
    return os.path.join(get_sdk_docs_root(), "doc")

def url_from_path(path):
    path = path.lstrip("/")
    return "file://"+"/"+"/".join(path.split(os.sep))+"/"

def get_base_url():
    return url_from_path(get_base_url_path())

class Link_Checker(HTMLParser.HTMLParser):
    def __init__(self, tester, filename, base_url):
        HTMLParser.HTMLParser.__init__(self)
        self.tester = tester
        self.filename = filename
        self.base_url = base_url
        self.errors = []

    def handle_starttag(self, tag, attrs):
        link = self.find_link(attrs)
        if link:
            self.validate_link(link)

    def handle_startendtag(self, tag, attrs):
        link = self.find_link(attrs)
        if link:
            self.validate_link(link)

    def find_link(self, attrs):
        attrs = dict(attrs)
        href = attrs.get('href', '')
        if href:
            parsed = urlparse.urlparse(href)
            if not parsed.scheme:
                return href
        src = attrs.get('src', '')
        if src:
            parsed = urlparse.urlparse(src)
            if not parsed.scheme:
                return src

    def validate_link(self, link):
        parsed = urlparse.urlparse(link)
        # there should not be any file:// URLs
        self.tester.assertNotEqual(parsed.scheme, "file")
        # any other absolute URLs will not be checked
        if parsed.scheme:
            return
        current_path_as_url = url_from_path(os.path.dirname(self.filename))
        # otherwise try to open the file at: baseurl + path
        absolute_url = current_path_as_url + parsed.path
        try:
            urllib.urlopen(absolute_url)
        except IOError:
            self.errors.append(self.filename + "\n    " + absolute_url)

class Generate_Docs_Tests(unittest.TestCase):

    def test_generate_static_docs(self):

        def cleanup():
            shutil.rmtree(get_base_url_path())
            tgz.close()
            os.remove(tar_filename)
            generate.clean_generated_docs(os.path.join(env_root, "doc"))

        # make sure we start clean
        if os.path.exists(get_base_url_path()):
            shutil.rmtree(get_base_url_path())
        # generate a doc tarball, and extract it
        base_url = get_base_url()
        tar_filename = generate.generate_static_docs(env_root)
        tgz = tarfile.open(tar_filename)
        tgz.extractall(get_sdk_docs_root())
        broken_links = []
        # get each HTML file...
        for root, subFolders, filenames in os.walk(get_sdk_docs_root()):
            for filename in filenames:
                if not filename.endswith(".html"):
                    continue
                if root.endswith("static-files"):
                    continue
                filename = os.path.join(root, filename)
                # ...and feed it to the link checker
                linkChecker = Link_Checker(self, filename, base_url)
                linkChecker.feed(open(filename, "r").read())
                broken_links.extend(linkChecker.errors)
        if broken_links:
            print
            print "The following links are broken:"
            for broken_link in sorted(broken_links):
                print " "+ broken_link

            cleanup()
            self.fail("%d links are broken" % len(broken_links))

        cleanup()

    def test_generate_docs(self):
        test_root = get_test_root()
        docs_root = os.path.join(test_root, "doc")
        static_files_src = os.path.join(env_root, "doc", "static-files")
        static_files_dst = os.path.join(test_root, "doc", "static-files")
        if os.path.exists(static_files_dst):
            shutil.rmtree(static_files_dst)
        shutil.copytree(static_files_src, static_files_dst)
        generate.clean_generated_docs(docs_root)
        new_digest = self.check_generate_regenerate_cycle(test_root, INITIAL_FILESET)
        # touching an MD file under sdk **does** cause a regenerate
        os.utime(os.path.join(test_root, "doc", "module-source", "sdk", "aardvark-feeder.md"), None)
        new_digest = self.check_generate_regenerate_cycle(test_root, INITIAL_FILESET, new_digest)
        # touching a non MD file under sdk **does not** cause a regenerate
        os.utime(os.path.join(test_root, "doc", "module-source", "sdk", "not_a_doc.js"), None)
        self.check_generate_is_skipped(test_root, INITIAL_FILESET, new_digest)
        # touching a non MD file under static-files **does not** cause a regenerate
        os.utime(os.path.join(docs_root, "static-files", "css", "sdk-docs.css"), None)
        new_digest = self.check_generate_is_skipped(test_root, INITIAL_FILESET, new_digest)
        # touching an MD file under dev-guide **does** cause a regenerate
        os.utime(os.path.join(docs_root, "dev-guide-source", "index.md"), None)
        new_digest = self.check_generate_regenerate_cycle(test_root, INITIAL_FILESET, new_digest)
        # adding a file **does** cause a regenerate
        open(os.path.join(docs_root, "dev-guide-source", "extra.md"), "w").write("some content")
        new_digest = self.check_generate_regenerate_cycle(test_root, EXTENDED_FILESET, new_digest)
        # deleting a file **does** cause a regenerate
        os.remove(os.path.join(docs_root, "dev-guide-source", "extra.md"))
        new_digest = self.check_generate_regenerate_cycle(test_root, INITIAL_FILESET, new_digest)
        # remove the files
        generate.clean_generated_docs(docs_root)
        shutil.rmtree(static_files_dst)

    def check_generate_is_skipped(self, test_root, files_to_expect, initial_digest):
        generate.generate_docs(test_root, stdout=StringIO.StringIO())
        docs_root = os.path.join(test_root, "doc")
        for file_to_expect in files_to_expect:
            self.assertTrue(os.path.exists(os.path.join(docs_root, *file_to_expect)))
        self.assertTrue(initial_digest == open(os.path.join(docs_root, "status.md5"), "r").read())

    def check_generate_regenerate_cycle(self, test_root, files_to_expect, initial_digest = None):
        # test that if we generate, files are getting generated
        generate.generate_docs(test_root, stdout=StringIO.StringIO())
        docs_root = os.path.join(test_root, "doc")
        for file_to_expect in files_to_expect:
            self.assertTrue(os.path.exists(os.path.join(docs_root, *file_to_expect)), os.path.join(docs_root, *file_to_expect) + "not found")
        if initial_digest:
            self.assertTrue(initial_digest != open(os.path.join(docs_root, "status.md5"), "r").read())
        # and that if we regenerate, nothing changes...
        new_digest = open(os.path.join(docs_root, "status.md5"), "r").read()
        self.check_generate_is_skipped(test_root, files_to_expect, new_digest)
        return new_digest

if __name__ == '__main__':
    unittest.main()
