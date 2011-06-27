#!/usr/bin/python
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
# The Original Code is SpiderMonkey JavaScript engine.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Graydon Hoare <graydon@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#
# The output of this script is a splicemap.
#
# A splicemap is used by the mercurial 'convert' extension to direct its
# splicing operation.
#
# The script assumes you already have a destination repository
# containing a conversion somewhere in its history, but that you've
# possibly mixed some new commits on top of that conversion. It outputs
# a splicemap that picks up the conversion where it left off (with the
# first descendant, in the source repo, of the src rev given by --start,
# if or rather the first descendant that would be included by the
# conversion's filemap) and connects them to the current tip of the
# destination repo.
#

from mercurial import ui, hg
from hgext.convert.filemap import filemapper
from optparse import OptionParser

import sys

parser = OptionParser()

parser.add_option("-s", "--src", dest="src",
                  help="source repository", metavar="REPO")

parser.add_option("-d", "--dst", dest="dst",
                  help="destination repository", metavar="REPO")

parser.add_option("-t", "--start", dest="start",
                  help="starting revid in source repository", metavar="REV")

parser.add_option("-f", "--filemap", dest="filemap",
                  help="filemap used in conversion", metavar="PATH")

(options, args) = parser.parse_args()

if not (options.src and options.dst and options.start):
    parser.print_help()
    exit(1)

u = ui.ui()

src_repo = hg.repository(u, options.src)
dst_repo = hg.repository(u, options.dst)

fm = None
if options.filemap:
    fm = filemapper(u, options.filemap)

last_converted_src = src_repo[options.start]

dst_tip = dst_repo.changectx(dst_repo.changelog.tip()).hex()
revs = last_converted_src.children()

while len(revs) != 0:
    tmp = revs
    revs = []
    for child in tmp:
        for f in child.files():
            if (not fm) or fm(f):
                u.write("%s %s\n" % (child.hex(), dst_tip))
                exit(0);
        revs.extend(child.children())

sys.stderr.write("No candidate child found in source repository\n")
exit(1)
