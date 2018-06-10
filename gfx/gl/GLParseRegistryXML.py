#!/usr/bin/env python
# coding=utf8


################################################################################
# TUTORIAL
# This script will generate GLConsts.h
#
# Step 1:
#   Download the last gl.xml, egl.xml, glx.xml and wgl.xml from
#   http://www.opengl.org/registry/#specfiles into a directory of your choice
#
# Step 2:
#   Execute this script ./GLParseRegistryXML.py [your dir containing XML files]
#
# Step 3:
#   Do not add the downloaded XML in the patch
#
# Step 4:
#   Enjoy =)
#
################################################################################

# includes
from __future__ import print_function
import os
import sys
import xml.etree.ElementTree


################################################################################
# export management

class GLConstHeader:
    def __init__(self, f):
        self.f = f

    def write(self, arg):
        if isinstance(arg, list):
            self.f.write('\n'.join(arg) + '\n')
        elif isinstance(arg, (int, long)):
            self.f.write('\n' * arg)
        else:
            self.f.write(str(arg) + '\n')

    def formatFileBegin(self):
        self.write([
            '/* This Source Code Form is subject to the terms of the Mozilla Public',
            ' * License, v. 2.0. If a copy of the MPL was not distributed with this',
            ' * file, You can obtain one at http://mozilla.org/MPL/2.0/. */',
            '',
            '#ifndef GLCONSTS_H_',
            '#define GLCONSTS_H_',
            '',
            '/**',
            ' * GENERATED FILE, DO NOT MODIFY DIRECTLY.',
            ' * This is a file generated directly from the official OpenGL registry',
            ' * xml available http://www.opengl.org/registry/#specfiles.',
            ' *',
            ' * To generate this file, see tutorial in \'GLParseRegistryXML.py\'.',
            ' */',
            ''
        ])

    def formatLibBegin(self, lib):
        # lib would be 'GL', 'EGL', 'GLX' or 'WGL'
        self.write('// ' + lib)

    def formatLibConstant(self, lib, name, value):
        # lib would be 'GL', 'EGL', 'GLX' or 'WGL'
        # name is the name of the const (example: MAX_TEXTURE_SIZE)
        # value is the value of the const (example: 0xABCD)

        define = '#define LOCAL_' + lib + '_' + name
        whitespace = 60 - len(define)

        if whitespace < 0:
            whitespace = whitespace % 8

        self.write(define + ' ' * whitespace + ' ' + value)

    def formatLibEnd(self, lib):
        # lib would be 'GL', 'EGL', 'GLX' or 'WGL'
        self.write(2)

    def formatFileEnd(self):
        self.write([
            '',
            '#endif // GLCONSTS_H_'
        ])


################################################################################
# underground code

def getScriptDir():
    return os.path.dirname(__file__) + '/'


def getXMLDir():
    if len(sys.argv) == 1:
        return './'

    dirPath = sys.argv[1]
    if dirPath[-1] != '/':
        dirPath += '/'

    return dirPath


class GLConst:
    def __init__(self, lib, name, value, type):
        self.lib = lib
        self.name = name
        self.value = value
        self.type = type


class GLDatabase:

    LIBS = ['GL', 'EGL', 'GLX', 'WGL']

    def __init__(self):
        self.consts = {}
        self.libs = set(GLDatabase.LIBS)
        self.vendors = set(['EXT', 'ATI'])
        # there is no vendor="EXT" and vendor="ATI" in gl.xml,
        # so we manualy declare them

    def loadXML(self, path):
        xmlPath = getXMLDir() + path

        if not os.path.isfile(xmlPath):
            print('missing file "' + xmlPath + '"')
            return False

        tree = xml.etree.ElementTree.parse(xmlPath)
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

        return True

    def exportConsts(self, path):
        with open(getScriptDir() + path, 'w') as f:

            headerFile = GLConstHeader(f)
            headerFile.formatFileBegin()

            constNames = self.consts.keys()
            constNames.sort()

            for lib in GLDatabase.LIBS:
                headerFile.formatLibBegin(lib)

                for constName in constNames:
                    const = self.consts[constName]

                    if const.lib != lib:
                        continue

                    headerFile.formatLibConstant(lib, const.name, const.value)

                headerFile.formatLibEnd(lib)

            headerFile.formatFileEnd()


glDatabase = GLDatabase()

success = glDatabase.loadXML('gl.xml')
success = success and glDatabase.loadXML('egl.xml')
success = success and glDatabase.loadXML('glx.xml')
success = success and glDatabase.loadXML('wgl.xml')

if success:
    glDatabase.exportConsts('GLConsts.h')
