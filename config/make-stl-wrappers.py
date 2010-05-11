# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Jones <jones.chris.g@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import os, re, string, sys

def find_in_path(file, searchpath):
    for dir in searchpath.split(os.pathsep):
        f = os.path.join(dir, file)
        if os.path.exists(f):
            return f
    return ''

def header_path(header, compiler):
    if compiler == 'gcc':
        # we use include_next on gcc
        return header
    elif compiler == 'msvc':
        return find_in_path(header, os.environ.get('INCLUDE', ''))
    else:
        # hope someone notices this ...
        raise NotImplementedError, compiler

def is_comment(line):
    return re.match(r'\s*#.*', line)

def main(outdir, compiler, template_file, header_list_file):
    if not os.path.isdir(outdir):
        os.mkdir(outdir)

    template = open(template_file, 'r').read()
    path_to_new = header_path('new', compiler)

    for header in open(header_list_file, 'r'):
        header = header.rstrip()
        if 0 == len(header) or is_comment(header):
            continue

        path = header_path(header, compiler)
        try:
            f = open(os.path.join(outdir, header), 'w')
            f.write(string.Template(template).substitute(HEADER=header,
                                                         HEADER_PATH=path,
                                                         NEW_HEADER_PATH=path_to_new))
        finally:
            f.close()


if __name__ == '__main__':
    if 5 != len(sys.argv):
        print >>sys.stderr, """Usage:
  python %s OUT_DIR ('msvc'|'gcc') TEMPLATE_FILE HEADER_LIST_FILE
"""% (sys.argv[0])
        sys.exit(1)

    main(*sys.argv[1:])
