# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

if len(sys.argv) != 4:
    print >> sys.stderr, "Usage: copy_source.py " \
                         "<topsrcdir> <source directory> <target directory>"
    sys.exit(1)

topsrcdir = sys.argv[1]
source_dir = sys.argv[2]
target_dir = sys.argv[3]

print """
DEPTH     = ..
topsrcdir = %(topsrcdir)s
srcdir    = %(topsrcdir)s/addon-sdk
VPATH     = %(topsrcdir)s/addon-sdk

include $(topsrcdir)/config/config.mk
""" % {'topsrcdir': topsrcdir}

real_source = source_dir.replace('/', os.sep)
if not os.path.exists(real_source):
    print >> sys.stderr, "Error: Missing source file %s" % real_source
    sys.exit(1)
elif not os.path.isdir(real_source):
    print >> sys.stderr, "Error: Source %s is not a directory" % real_source
    sys.exit(1)
for dirpath, dirnames, filenames in os.walk(real_source):
    if not filenames:
        continue
    dirpath = dirpath.replace(os.sep, '/')
    relative = dirpath[len(source_dir):]
    varname = "COMMONJS%s" % relative.replace('/', '_')
    print "%s_FILES = \\" % varname
    for name in filenames:
        print "  %s/%s \\" % (dirpath, name)
    print "  $(NULL)"
    print "%s_DEST = %s%s" % (varname, target_dir, relative)
    print "INSTALL_TARGETS += %s\n" % varname

print "include $(topsrcdir)/config/rules.mk"
