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
# The Original Code is Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000, 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mark Hammond <mhammond@skippinet.com.au> (original author)
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

# This is a demo is how to use the xpcom.server "tracer" facility.
#
# This demo installs a tracer that uses the Python profiler.  It then
# creates the Python test component, and references some methods
# and properties.  It then dumps the profile statistics.

# This same technique could also be used for debugging, for example.

import profile

p = profile.Profile()
getters = {}
setters = {}

# A wrapper around a function - looks like a function,
# but actually profiles the delegate.
class TracerDelegate:
    def __init__(self, callme):
        self.callme = callme
    def __call__(self, *args):
        return p.runcall(self.callme, *args)

# A wrapper around each of our XPCOM objects.  All PyXPCOM calls
# in are made on this object, which creates a TracerDelagate around
# every function.  As the function is called, it collects profile info.
class Tracer:
    def __init__(self, ob):
        self.__dict__['_ob'] = ob
    def __repr__(self):
        return "<Tracer around %r>" % (self._ob,)
    def __str__(self):
        return "<Tracer around %r>" % (self._ob,)
    def __getattr__(self, attr):
        ret = getattr(self._ob, attr) # Attribute error just goes up
        if callable(ret):
            return TracerDelegate(ret)
        else:
            if not attr.startswith("_com_") and not attr.startswith("_reg_"):
                getters[attr] = getters.setdefault(attr,0) + 1
            return ret
    def __setattr__(self, attr, val):
        if self.__dict__.has_key(attr):
            self.__dict__[attr] = val
            return
        setters[attr] = setters.setdefault(attr,0) + 1
        setattr(self._ob, attr, val)

# Installed as a global XPCOM function that if exists, will be called
# to wrap each XPCOM object created.
def MakeTracer(ob):
    # In some cases we may be asked to wrap ourself, so handle that.
    if isinstance(ob, Tracer):
        return ob
    return Tracer(ob)

def test():
    import xpcom.server, xpcom.components
    xpcom.server.tracer = MakeTracer
    contractid = "Python.TestComponent"
    for i in range(100):
        c = xpcom.components.classes[contractid].createInstance().queryInterface(xpcom.components.interfaces.nsIPythonTestInterface)
        c.boolean_value = 0
        a = c.boolean_value
        c.do_boolean(0,1)
    print "Finshed"
    p.print_stats()
    print "%-30s%s" % ("Attribute Gets", "Number")
    print "-" * 36
    for name, num in getters.items():
        print "%-30s%d" % (name, num)
    print "%-30s%s" % ("Attribute Sets", "Number")
    print "-" * 36
    for name, num in setters.items():
        print "%-30s%d" % (name, num)

test()
