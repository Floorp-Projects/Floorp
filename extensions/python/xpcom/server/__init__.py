# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

# The xpcom.server package.

from policy import DefaultPolicy
from xpcom import _xpcom

# We define the concept of a single "tracer" object - similar to the single
# Python "trace hook" for debugging.  Someone can set
# xpcom.server.tracer to some class/function, and it will be used in place
# of the real xpcom object.  Presumably this "trace" object will delegate
# to the real object, but presumably also taking some other action, such
# as calling a profiler or debugger.
tracer = None

# Wrap an instance in an interface (via a policy)
def WrapObject(ob, iid, policy = None):
    """Called by the framework to attempt to wrap
    an object in a policy.
    If iid is None, it will use the first interface the object indicates it supports.
    """
    if policy is None:
        policy = DefaultPolicy
    if tracer is not None:
        ob = tracer(ob)
    return _xpcom.WrapObject(policy( ob, iid ), iid)

# Create the main module for the Python loader.
# This is a once only init process, and the returned object
# if used to load all other Python components.

# This means that we keep all factories, modules etc implemented in
# Python!
def NS_GetModule( serviceManager, nsIFile ):
    import loader
    iid = _xpcom.IID_nsIModule
    return WrapObject(loader.MakePythonComponentLoaderModule(serviceManager, nsIFile), iid)
