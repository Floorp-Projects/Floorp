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
#        Mark Hammond <MarkH@ActiveState.com> (original author)
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

# Test our support for the interfaces defined in nsISupportsPrimitives.idl
#
# The framework supports nsISupportsCString and nsISupportsString, but
# only if our class doesnt provide explicit support.

from xpcom import components

class NoSupportsString:
    _com_interfaces_ = [components.interfaces.nsISupports]
    pass

class ImplicitSupportsString:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __str__(self):
        return "<MyImplicitStrObject>"

class ExplicitSupportsString:
    _com_interfaces_ = [components.interfaces.nsISupports, components.interfaces.nsISupportsCString]
    # __str__ will be ignored by XPCOM, as we have _explicit_ support.
    def __str__(self):
        return "<MyImplicitStrObject>"
    # This is the one that will be used.
    def toString(self):
        return "<MyExplicitStrObject>"

class ImplicitSupportsInt:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __int__(self):
        return 99

class ExplicitSupportsInt:
    _com_interfaces_ = [components.interfaces.nsISupportsPRInt32]
    def get_data(self):
        return 99

class ImplicitSupportsLong:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __long__(self):
        return 99L

class ExplicitSupportsLong:
    _com_interfaces_ = [components.interfaces.nsISupportsPRInt64]
    def get_data(self):
        return 99

class ExplicitSupportsFloat:
    _com_interfaces_ = [components.interfaces.nsISupportsDouble]
    def get_data(self):
        return 99.99

class ImplicitSupportsFloat:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __float__(self):
        return 99.99

def test():
    import xpcom.server, xpcom.client
    ob = xpcom.server.WrapObject( NoSupportsString(), components.interfaces.nsISupports)
    if not str(ob).startswith("<XPCOM "):
        raise RuntimeError, "Wrong str() value: %s" % (ob,)

    ob = xpcom.server.WrapObject( ImplicitSupportsString(), components.interfaces.nsISupports)
    if str(ob) != "<MyImplicitStrObject>":
        raise RuntimeError, "Wrong str() value: %s" % (ob,)

    ob = xpcom.server.WrapObject( ExplicitSupportsString(), components.interfaces.nsISupports)
    if str(ob) != "<MyExplicitStrObject>":
        raise RuntimeError, "Wrong str() value: %s" % (ob,)

    # Try our conversions.
    try:
        int(ob)
        raise RuntimeError, "Expected to get a ValueError converting this COM object to an int"
    except ValueError:
        pass
    ob = xpcom.server.WrapObject( ExplicitSupportsInt(), components.interfaces.nsISupports)
    if int(ob) != 99:
        raise RuntimeError, "Bad value: %s" % (int(ob),)
    if float(ob) != 99.0:
        raise RuntimeError, "Bad value: %s" % (float(ob),)

    ob = xpcom.server.WrapObject( ImplicitSupportsInt(), components.interfaces.nsISupports)
    if int(ob) != 99:
        raise RuntimeError, "Bad value: %s" % (int(ob),)
    if float(ob) != 99.0:
        raise RuntimeError, "Bad value: %s" % (float(ob),)

    ob = xpcom.server.WrapObject( ExplicitSupportsLong(), components.interfaces.nsISupports)
    if long(ob) != 99 or not repr(long(ob)).endswith("L"):
        raise RuntimeError, "Bad value: %s" % (repr(long(ob)),)
    if float(ob) != 99.0:
        raise RuntimeError, "Bad value: %s" % (float(ob),)

    ob = xpcom.server.WrapObject( ImplicitSupportsLong(), components.interfaces.nsISupports)
    if long(ob) != 99 or not repr(long(ob)).endswith("L"):
        raise RuntimeError, "Bad value: %s" % (repr(long(ob)),)
    if float(ob) != 99.0:
        raise RuntimeError, "Bad value: %s" % (float(ob),)

    ob = xpcom.server.WrapObject( ExplicitSupportsFloat(), components.interfaces.nsISupports)
    if float(ob) != 99.99:
        raise RuntimeError, "Bad value: %s" % (float(ob),)
    if int(ob) != 99:
        raise RuntimeError, "Bad value: %s" % (int(ob),)

    ob = xpcom.server.WrapObject( ImplicitSupportsFloat(), components.interfaces.nsISupports)
    if float(ob) != 99.99:
        raise RuntimeError, "Bad value: %s" % (float(ob),)
    if int(ob) != 99:
        raise RuntimeError, "Bad value: %s" % (int(ob),)

    print "The nsISupports primitive interface tests appeared to work"
test()
