# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, re, errno
import markdown
import cgi

from cuddlefish import packaging
from cuddlefish.docs import apirenderer
from cuddlefish._version import get_versions

INDEX_PAGE = '/doc/static-files/base.html'
BASE_URL_INSERTION_POINT = '<base '
VERSION_INSERTION_POINT = '<div id="version">'
MODULE_INDEX_INSERTION_POINT = '<ul id="module-index">'
THIRD_PARTY_MODULE_SUMMARIES = '<ul id="third-party-module-summaries">'
HIGH_LEVEL_MODULE_SUMMARIES = '<ul id="high-level-module-summaries">'
LOW_LEVEL_MODULE_SUMMARIES = '<ul id="low-level-module-summaries">'
CONTENT_ID = '<div id="main-content">'
TITLE_ID = '<title>'
DEFAULT_TITLE = 'Add-on SDK Documentation'

def tag_wrap(text, tag, attributes={}):
    result = '\n<' + tag
    for name in attributes.keys():
        result += ' ' + name + '=' + '"' + attributes[name] + '"'
    result +='>' + text + '</'+ tag + '>\n'
    return result

def insert_after(target, insertion_point_id, text_to_insert):
    insertion_point = target.find(insertion_point_id) + len(insertion_point_id)
    return target[:insertion_point] + text_to_insert + target[insertion_point:]

class WebDocs(object):
    def __init__(self, root, module_list, version=get_versions()["version"], base_url = None):
        self.root = root
        self.module_list = module_list
        self.version = version
        self.pkg_cfg = packaging.build_pkg_cfg(root)
        self.packages_json = packaging.build_pkg_index(self.pkg_cfg)
        self.base_page = self._create_base_page(root, base_url)

    def create_guide_page(self, path):
        md_content = unicode(open(path, 'r').read(), 'utf8')
        guide_content = markdown.markdown(md_content)
        return self._create_page(guide_content)

    def create_module_page(self, module_info):
        path, ext = os.path.splitext(module_info.source_path_and_filename())
        md_path = path + '.md'
        module_content = apirenderer.md_to_div(md_path, module_info.name())
        stability = module_info.metadata.get("stability", "undefined")
        stability_note = tag_wrap(stability, "a", {"class":"stability-note stability-" + stability, \
                                                     "href":"dev-guide/guides/stability.html"})
        module_content = stability_note + module_content
        return self._create_page(module_content)

    def create_module_index(self, path, module_list):
        md_content = unicode(open(path, 'r').read(), 'utf8')
        index_content = markdown.markdown(md_content)
        module_list_content = self._make_module_text(module_list)
        index_content = insert_after(index_content, MODULE_INDEX_INSERTION_POINT, module_list_content)
        return self._create_page(index_content)

    def _create_page(self, page_content):
        page = self._insert_title(self.base_page, page_content)
        page = insert_after(page, CONTENT_ID, page_content)
        return page.encode('utf8')

    def _make_module_text(self, module_list):
        module_text = ''
        for module in module_list:
            module_link = tag_wrap(module.name(), 'a', \
                {'href': "/".join(["modules", module.relative_url()])})
            module_list_item = tag_wrap(module_link, "li")
            module_text += module_list_item
        return module_text

    def _create_base_page(self, root, base_url):
        base_page = unicode(open(root + INDEX_PAGE, 'r').read(), 'utf8')
        if base_url:
            base_tag = 'href="' + base_url + '"'
            base_page = insert_after(base_page, BASE_URL_INSERTION_POINT, base_tag)
        base_page = insert_after(base_page, VERSION_INSERTION_POINT, "Version " + self.version)

        third_party_module_list = [module_info for module_info in self.module_list if module_info.level() == "third-party"]
        third_party_module_text = self._make_module_text(third_party_module_list)
        base_page = insert_after(base_page, \
            THIRD_PARTY_MODULE_SUMMARIES, third_party_module_text)

        high_level_module_list = [module_info for module_info in self.module_list if module_info.level() == "high"]
        high_level_module_text = self._make_module_text(high_level_module_list)
        base_page = insert_after(base_page, \
            HIGH_LEVEL_MODULE_SUMMARIES, high_level_module_text)

        low_level_module_list = [module_info for module_info in self.module_list if module_info.level() == "low"]
        low_level_module_text = self._make_module_text(low_level_module_list)
        base_page = insert_after(base_page, \
            LOW_LEVEL_MODULE_SUMMARIES, low_level_module_text)
        return base_page

    def _insert_title(self, target, content):
        match = re.search('<h1>.*</h1>', content)
        if match:
            title = match.group(0)[len('<h1>'):-len('</h1>')] + ' - ' + \
                DEFAULT_TITLE
        else:
            title = DEFAULT_TITLE
        target = insert_after(target, TITLE_ID, title)
        return target
