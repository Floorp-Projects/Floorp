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
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000, 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <markh@activestate.com> (original author)
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

# test_weakreferences.py - Test our weak reference implementation.
from xpcom import components, _xpcom
import xpcom.server, xpcom.client

try:
    from sys import gettotalrefcount
except ImportError:
    # Not a Debug build - assume no references (can't be leaks then :-)
    gettotalrefcount = lambda: 0

num_alive = 0

class koTestSimple:
    _com_interfaces_ = [components.interfaces.nsIInputStream]
    def __init__(self):
        global num_alive
        num_alive += 1
    def __del__(self):
        global num_alive
        num_alive -= 1
    def close( self ):
        pass

def test():
    ob = xpcom.server.WrapObject( koTestSimple(), components.interfaces.nsIInputStream)

    if num_alive != 1: raise RuntimeError, "Eeek - there are %d objects alive" % (num_alive,)

    # Check we can create a weak reference to our object.
    wr = xpcom.client.WeakReference(ob)
    if num_alive != 1: raise RuntimeError, "Eeek - there are %d objects alive" % (num_alive,)

    # Check we can call methods via the weak reference.
    if wr() is None: raise RuntimeError, "Our weak-reference is returning None before it should!"
    wr().close()

    ob  = None # This should kill the object.    
    if num_alive != 0: raise RuntimeError, "Eeek - there are %d objects alive" % (num_alive,)
    if wr() is not None: raise RuntimeError, "Our weak-reference is not returning None when it should!"

    # Now a test that we can get a _new_ interface from the weak reference - ie,
    # an IID the real object has never previously been queried for
    # (this behaviour previously caused a bug - never again ;-)
    ob = xpcom.server.WrapObject( koTestSimple(), components.interfaces.nsISupports)
    if num_alive != 1: raise RuntimeError, "Eeek - there are %d objects alive" % (num_alive,)
    wr = xpcom.client.WeakReference(ob, components.interfaces.nsIInputStream)
    if num_alive != 1: raise RuntimeError, "Eeek - there are %d objects alive" % (num_alive,)
    wr() # This would die once upon a time ;-)
    ob  = None # This should kill the object.    
    if num_alive != 0: raise RuntimeError, "Eeek - there are %d objects alive" % (num_alive,)
    if wr() is not None: raise RuntimeError, "Our weak-reference is not returning None when it should!"

def test_refcount(num_loops=-1):
    # Do the test lots of times - can help shake-out ref-count bugs.
    if num_loops == -1: num_loops = 10
    for i in xrange(num_loops):
        test()

        if i==0:
            # First loop is likely to "leak" as we cache things.
            # Leaking after that is a problem.
            num_refs = gettotalrefcount()

    lost = gettotalrefcount() - num_refs
    # Sometimes we get spurious counts off by 1 or 2.
    # This can't indicate a real leak, as we have looped
    # more than twice!
    if abs(lost)>2:
        print "*** Lost %d references" % (lost,)
    
test_refcount()

print "Weak-reference tests appear to have worked!"
if __name__=='__main__':
    _xpcom.NS_ShutdownXPCOM()
    ni = xpcom._xpcom._GetInterfaceCount()
    ng = xpcom._xpcom._GetGatewayCount()
    if ni or ng:
        print "********* WARNING - Leaving with %d/%d objects alive" % (ni,ng)
