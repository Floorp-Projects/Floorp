#!/usr/bin/env python3

'''
This script will regenerate and update GLConsts.h.

Step 1:
  Download the last gl.xml, egl.xml, glx.xml and wgl.xml from
  http://www.opengl.org/registry/#specfiles into some XML_DIR:
    wget https://www.khronos.org/registry/OpenGL/xml/gl.xml
    wget https://www.khronos.org/registry/OpenGL/xml/glx.xml
    wget https://www.khronos.org/registry/OpenGL/xml/wgl.xml
    wget https://www.khronos.org/registry/EGL/api/egl.xml

Step 2:
  `py ./GLConsts.py <XML_DIR>`

Step 3:
  Do not add the downloaded XML in the patch

Step 4:
  Enjoy =)
'''

# includes
from typing import List # mypy!

import pathlib
import sys
import xml.etree.ElementTree

# -

(_, XML_DIR_STR) = sys.argv
XML_DIR = pathlib.Path(XML_DIR_STR)

# -

HEADER = b'''
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// clang-format off

#ifndef GLCONSTS_H_
#define GLCONSTS_H_

/**
 * GENERATED FILE, DO NOT MODIFY DIRECTLY.
 * This is a file generated directly from the official OpenGL registry
 * xml available http://www.opengl.org/registry/#specfiles.
 *
 * To generate this file, see tutorial in \'GLParseRegistryXML.py\'.
 */
'''[1:]

FOOTER = b'''
#endif // GLCONSTS_H_

// clang-format on
'''[1:]

# -

def format_lib_constant(lib, name, value):
    # lib would be 'GL', 'EGL', 'GLX' or 'WGL'
    # name is the name of the const (example: MAX_TEXTURE_SIZE)
    # value is the value of the const (example: 0xABCD)

    define = '#define LOCAL_' + lib + '_' + name
    whitespace = 60 - len(define)
    if whitespace < 0:
        whitespace = whitespace % 8

    return define + ' ' * whitespace + ' ' + value

# -

class GLConst:
    def __init__(self, lib, name, value, type):
        self.lib = lib
        self.name = name
        self.value = value
        self.type = type

# -

class GLDatabase:
    LIBS = ['GL', 'EGL', 'GLX', 'WGL']

    def __init__(self):
        self.consts = {}
        self.libs = set(GLDatabase.LIBS)
        self.vendors = set(['EXT', 'ATI'])
        # there is no vendor="EXT" and vendor="ATI" in gl.xml,
        # so we manualy declare them

    def load_xml(self, xml_path):
        tree = xml.etree.ElementTree.parse(xml_path)
        root = tree.getroot()

        for enums in root.iter('enums'):
            vendor = enums.get('vendor')
            if not vendor:
                # there some standart enums that do have the vendor attribute,
                # so we fake them as ARB's enums
                vendor = 'ARB'

            if vendor not in self.vendors:
                # we map this new vendor in the vendors set.
                self.vendors.add(vendor)

            namespaceType = enums.get('type')

            for enum in enums:
                if enum.tag != 'enum':
                    # this is not an enum => we skip it
                    continue

                lib = enum.get('name').split('_')[0]

                if lib not in self.libs:
                    # unknown library => we skip it
                    continue

                name = enum.get('name')[len(lib) + 1:]
                value = enum.get('value')
                type = enum.get('type')

                if not type:
                    # if no type specified, we get the namespace's default type
                    type = namespaceType

                self.consts[lib + '_' + name] = GLConst(lib, name, value, type)

# -

db = GLDatabase()
db.load_xml(XML_DIR / 'gl.xml')
db.load_xml(XML_DIR / 'glx.xml')
db.load_xml(XML_DIR / 'wgl.xml')
db.load_xml(XML_DIR / 'egl.xml')

# -

lines: List[str] = []

keys = sorted(db.consts.keys())

for lib in db.LIBS:
    lines.append('// ' + lib)

    for k in keys:
        const = db.consts[k]

        if const.lib != lib:
            continue

        const_str = format_lib_constant(lib, const.name, const.value)
        lines.append(const_str)

    lines.append('')

# -

b_lines: List[bytes] = [HEADER] + [x.encode() for x in lines] + [FOOTER]
b_data: bytes = b'\n'.join(b_lines)

dest = pathlib.Path('GLConsts.h')
dest.write_bytes(b_data)

print(f'Wrote {len(b_data)} bytes.') # Some indication that we're successful.
