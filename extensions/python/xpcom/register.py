
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
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <MarkH@ActiveState.com> (original author)
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

import os, sys

def addSelf(fname):
    #XXX the path hardcoding and env var reliance limits the usefulness
    #XXX I don't even know if this is used, or usable, at all anymore.
    try:
        mozSrc = os.environ['MOZ_SRC']
    except KeyError:
        print "register.py: need MOZ_SRC to be set"
        sys.exit(-1)
    try:
        komododist = os.environ['KOMODO']
    except KeyError:
        print "Set KOMODO"
        sys.exit(-1)

    bindir = os.path.join(mozSrc, "dist", "WIN32_D.OBJ", "bin")
    idldir = os.path.join(mozSrc, "dist", "idl")
    idl2dir = os.path.join(komododist, "SciMoz")
    componentdir = os.path.normpath(os.path.join(bindir, 'components'))

    base, ext = os.path.splitext(fname)
    idlfile = base+'.idl'
    pyfile = base+'.py'
    xptfile = base+'.xpt'
    if os.path.exists(idlfile):
        # IDL file of same name exists, assume it needs to be updated
        print r'%(bindir)s\xpidl -I %(idldir)s -I %(idl2dir)s -m typelib %(idlfile)s' % vars()
        os.system(r'%(bindir)s\xpidl -I %(idldir)s -I %(idl2dir)s -m typelib %(idlfile)s' % vars())
    print r'cp %(xptfile)s %(componentdir)s' % vars()
    os.system(r'cp %(xptfile)s %(componentdir)s' % vars())
    print 'cp %(pyfile)s %(componentdir)s' % vars()
    os.system('cp %(pyfile)s %(componentdir)s' % vars())
        
