# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

# test_weakreferences.py - Test our weak reference implementation.
from xpcom import components, _xpcom
import xpcom.server, xpcom.client

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

    
test()
print "Weak-reference tests appear to have worked!"
