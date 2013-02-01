# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import HTMLParser
import urlparse

def rewrite_links(env_root, sdk_docs_path, page, dest_path):
    dest_path_depth = len(dest_path.split(os.sep)) -1 # because dest_path includes filename
    docs_root_depth = len(sdk_docs_path.split(os.sep))
    relative_depth = dest_path_depth - docs_root_depth
    linkRewriter = LinkRewriter("../" * relative_depth)
    return linkRewriter.rewrite_links(page)

class LinkRewriter(HTMLParser.HTMLParser):
    def __init__(self, link_prefix):
        HTMLParser.HTMLParser.__init__(self)
        self.stack = []
        self.link_prefix = link_prefix

    def rewrite_links(self, page):
        self.feed(page)
        self.close()
        page = ''.join(self.stack)
        self.stack = []
        return page

    def handle_decl(self, decl):
        self.stack.append("<!" + decl + ">")

    def handle_comment(self, decl):
        self.stack.append("<!--" + decl + "-->")

    def handle_starttag(self, tag, attrs):
        self.stack.append(self.__html_start_tag(tag, self._rewrite_link(attrs)))

    def handle_entityref(self, name):
        self.stack.append("&" + name + ";")

    def handle_endtag(self, tag):
        self.stack.append(self.__html_end_tag(tag))

    def handle_startendtag(self, tag, attrs):
        self.stack.append(self.__html_startend_tag(tag, self._rewrite_link(attrs)))

    def _update_attribute(self, attr_name, attrs):
        attr_value = attrs.get(attr_name, '')
        if attr_value:
            parsed = urlparse.urlparse(attr_value)
            if not parsed.scheme:
                attrs[attr_name] = self.link_prefix + attr_value

    def _rewrite_link(self, attrs):
        attrs = dict(attrs)
        self._update_attribute('href', attrs)
        self._update_attribute('src', attrs)
        self._update_attribute('action', attrs)
        return attrs

    def handle_data(self, data):
        self.stack.append(data)

    def __html_start_tag(self, tag, attrs):
        return '<%s%s>' % (tag, self.__html_attrs(attrs))

    def __html_startend_tag(self, tag, attrs):
        return '<%s%s/>' % (tag, self.__html_attrs(attrs))

    def __html_end_tag(self, tag):
        return '</%s>' % (tag)

    def __html_attrs(self, attrs):
        _attrs = ''
        if attrs:
            _attrs = ' %s' % (' '.join([('%s="%s"' % (k,v)) for k,v in dict(attrs).iteritems()]))
        return _attrs
