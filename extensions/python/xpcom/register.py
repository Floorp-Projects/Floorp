# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

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
        
