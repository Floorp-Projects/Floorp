# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
import shutil

from cuddlefish.tests import env_root
from cuddlefish.docs import webdocs
from cuddlefish.docs.documentationitem import get_module_list
from cuddlefish.docs.documentationitem import get_devguide_list

class WebDocTests(unittest.TestCase):

    def test_create_guide1_doc(self):
        root = os.path.join(os.getcwd() + \
            '/python-lib/cuddlefish/tests/static-files')
        self.copy_static_files(root)
        module_list = get_module_list(root)
        web_docs = webdocs.WebDocs(root, module_list)
        guide = web_docs.create_guide_page(os.path.join(\
            root + '/doc/dev-guide-source/index.md'))
        self.rm_static_files(root)
        self._test_common_contents(guide)
        self.assertTrue(\
            '<title>An Imposing Title - Add-on SDK Documentation</title>'\
            in guide)
        self.assertTrue('<p><em>Some words!</em></p>'\
            in guide)
        self.assertTrue('<div id="version">Version '\
            in guide)


    def test_create_guide2_doc(self):
        root = os.path.join(os.getcwd() + \
            '/python-lib/cuddlefish/tests/static-files')
        self.copy_static_files(root)
        module_list = get_module_list(root)
        web_docs = webdocs.WebDocs(root, module_list)
        guide = web_docs.create_guide_page(os.path.join(\
            root + '/doc/dev-guide-source/no_h1.md'))
        self.rm_static_files(root)
        self._test_common_contents(guide)
        self.assertTrue('<title>Add-on SDK Documentation</title>'\
            in guide)
        self.assertTrue('<h2>A heading</h2>'\
            in guide)

    def test_create_module_doc(self):
        root = os.path.join(os.getcwd() + \
            '/python-lib/cuddlefish/tests/static-files')
        self.copy_static_files(root)
        module_list = get_module_list(root)
        test_module_info = False
        for module_info in module_list:
            if module_info.name() == "aardvark-feeder":
                test_module_info = module_info
                break
        self.assertTrue(test_module_info)
        test_stability = test_module_info.metadata.get("stability", "undefined")
        self.assertEqual(test_stability, "stable")
        web_docs = webdocs.WebDocs(root, module_list)
        module = web_docs.create_module_page(test_module_info)
        self.rm_static_files(root)
        self._test_common_contents(module)
        self.assertTrue(\
            '<title>aardvark-feeder - Add-on SDK Documentation</title>'\
            in module)
        self.assertTrue(\
            'class="stability-note stability-stable"'\
            in module)
        self.assertTrue(\
            '<h1>aardvark-feeder</h1>'\
            in module)
        self.assertTrue(\
            '<div class="module_description">'\
            in module)
        self.assertTrue(\
            '<p>The <code>aardvark-feeder</code> module simplifies feeding aardvarks.</p>'\
            in module)
        self.assertTrue(\
            '<h2 class="api_header">API Reference</h2>'\
            in module)
        self.assertTrue(\
            '<h3 class="api_header">Functions</h3>'\
            in module)
        self.assertTrue(\
            '<h4 class="api_name">feed(food)</h4>'\
            in module)
        self.assertTrue(
            '<p>Feed the aardvark.</p>'\
            in module)

    def _test_common_contents(self, doc):
        self.assertTrue(\
            '<a href="modules/sdk/anteater/anteater.html">anteater/anteater</a>' in doc)
        self.assertTrue(\
            '<a href="modules/sdk/aardvark-feeder.html">aardvark-feeder</a>' in doc)

    def static_files_dir(self, root):
        return os.path.join(root, "doc", "static-files")

    def copy_static_files(self, test_root):
        self.rm_static_files(test_root)
        static_files_src = self.static_files_dir(env_root)
        static_files_dst = self.static_files_dir(test_root)
        shutil.copytree(static_files_src, static_files_dst)

    def rm_static_files(self, test_root):
        if os.path.exists(self.static_files_dir(test_root)):
            shutil.rmtree(self.static_files_dir(test_root))

if __name__ == "__main__":
    unittest.main()
