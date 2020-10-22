# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import BaseHTTPServer
from SimpleHTTPServer import SimpleHTTPRequestHandler


class RequestHandler(SimpleHTTPRequestHandler, object):
    def translate_path(self, path):
        # Map autofill paths to their own directory
        autofillPath = "/formautofill"
        if (path.startswith(autofillPath)):
            path = "browser/extensions/formautofill/content" + \
                path[len(autofillPath):]
        else:
            path = "browser/components/payments/res" + path

        return super(RequestHandler, self).translate_path(path)


if __name__ == '__main__':
    BaseHTTPServer.test(RequestHandler, BaseHTTPServer.HTTPServer)
