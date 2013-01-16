#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

def add_libdir_to_path():
    from os.path import dirname, exists, join, realpath
    js_src_dir = dirname(dirname(realpath(sys.argv[0])))
    assert exists(join(js_src_dir,'jsapi.h'))
    sys.path.append(join(js_src_dir, 'lib'))
    sys.path.append(join(js_src_dir, 'tests', 'lib'))

add_libdir_to_path()

import jittests

if __name__ == '__main__':
    jittests.main(sys.argv[1:])
