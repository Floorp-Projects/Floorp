# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
#

# The xpcom.server package.

from policy import DefaultPolicy
from xpcom import _xpcom

# We define the concept of a single "tracer" object - similar to the single
# Python "trace hook" for debugging.  Someone can set
# xpcom.server.tracer to some class/function, and it will be used in place
# of the real xpcom object.  Presumably this "trace" object will delegate
# to the real object, but presumably also taking some other action, such
# as calling a profiler or debugger.
# tracer_unwrap is a function used to "unwrap" the tracer object.
# If is expected that tracer_unwrap will be called with an object
# previously returned by "tracer()".
tracer = tracer_unwrap = None

# Wrap an instance in an interface (via a policy)
def WrapObject(ob, iid, policy = None, bWrapClient = 1):
    """Called by the framework to attempt to wrap
    an object in a policy.
    If iid is None, it will use the first interface the object indicates it supports.
    """
    if policy is None:
        policy = DefaultPolicy
    if tracer is not None:
        ob = tracer(ob)
    return _xpcom.WrapObject(policy( ob, iid ), iid, bWrapClient)

# Unwrap a Python object back into the Python object
def UnwrapObject(ob):
    if ob is None:
        return None
    ret = _xpcom.UnwrapObject(ob)._obj_
    if tracer_unwrap is not None:
        ret = tracer_unwrap(ret)
    return ret

# Create the main module for the Python loader.
# This is a once only init process, and the returned object
# if used to load all other Python components.

# This means that we keep all factories, modules etc implemented in
# Python!
def NS_GetModule( serviceManager, nsIFile ):
    import loader
    iid = _xpcom.IID_nsIModule
    return WrapObject(loader.MakePythonComponentLoaderModule(serviceManager, nsIFile), iid, bWrapClient = 0)

def _shutdown():
    from policy import _shutdown
    _shutdown()
