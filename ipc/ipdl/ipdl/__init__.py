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

__all__ = [ 'gencxx', 'genipdl', 'parse', 'typecheck', 'writetofile' ]

import os, sys, errno
from cStringIO import StringIO

from ipdl.cgen import IPDLCodeGen
from ipdl.mgen import DependGen
from ipdl.lower import LowerToCxx
from ipdl.parser import Parser
from ipdl.type import TypeCheck

from ipdl.cxx.cgen import CxxCodeGen


def parse(specstring, filename='/stdin', includedirs=[ ], errout=sys.stderr):
    '''Return an IPDL AST if parsing was successful.  Print errors to |errout|
    if it is not.'''
    return Parser().parse(specstring, os.path.abspath(filename), includedirs, errout)


def typecheck(ast, errout=sys.stderr):
    '''Return True iff |ast| is well typed.  Print errors to |errout| if
    it is not.'''
    return TypeCheck().check(ast, errout)


def gencxx(ipdlfilename, ast, outheadersdir, outcppdir):
    headers, cpps = LowerToCxx().lower(ast)

    def resolveHeader(hdr):
        return [
            hdr, 
            os.path.join(
                outheadersdir,
                *([ns.name for ns in ast.protocol.namespaces] + [hdr.name]))
        ]
    def resolveCpp(cpp):
        return [ cpp, os.path.join(outcppdir, cpp.name) ]

    for ast, filename in ([ resolveHeader(hdr) for hdr in headers ]
                          + [ resolveCpp(cpp) for cpp in cpps ]):
        tempfile = StringIO()
        CxxCodeGen(tempfile).cgen(ast)
        writetofile(tempfile.getvalue(), filename)


def genipdl(ast, outdir):
    return IPDLCodeGen().cgen(ast)


def genm(ast, dir, filename):
    tempfile = StringIO()
    DependGen(tempfile).mgen(ast)
    filename = dir + "/" + os.path.basename(filename) + ".depends"
    writetofile(tempfile.getvalue(), filename)


def writetofile(contents, file):
    dir = os.path.dirname(file)

    # Guard against race condition by using Try instead
    # of |os.path.exists(dir) or os.makedirs(dir)|
    try:
        os.makedirs(dir)
    except OSError, ex:
        if ex.errno != errno.EEXIST:
            raise ex
        # else directory already exists. silently ignore and continue

    fd = open(file, 'wb')
    fd.write(contents)
    fd.close()
