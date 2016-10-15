# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Given a list of object files and library names, prints a library
descriptor to standard output'''

from __future__ import with_statement
import sys
import os
import expandlibs_config as conf
from expandlibs import LibDescriptor, isObject, ensureParentDir
from optparse import OptionParser

def generate(args):
    desc = LibDescriptor()
    for arg in args:
        if isObject(arg):
            if os.path.exists(arg):
                desc['OBJS'].append(os.path.abspath(arg))
            else:
                raise Exception("File not found: %s" % arg)
        elif os.path.splitext(arg)[1] == conf.LIB_SUFFIX:
            if os.path.exists(arg) or os.path.exists(arg + conf.LIBS_DESC_SUFFIX):
                desc['LIBS'].append(os.path.abspath(arg))
            else:
                raise Exception("File not found: %s" % arg)
    return desc

if __name__ == '__main__':
    parser = OptionParser()
    parser.add_option("-o", dest="output", metavar="FILE",
        help="send output to the given file")

    (options, args) = parser.parse_args()
    if not options.output:
        raise Exception("Missing option: -o")

    ensureParentDir(options.output)
    with open(options.output, 'w') as outfile:
        print >>outfile, generate(args)
