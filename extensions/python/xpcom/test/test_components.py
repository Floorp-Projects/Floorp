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

"""Tests the "xpcom.components" object.
"""

import xpcom.components

if not __debug__:
    raise RuntimeError, "This test uses assert, so must be run in debug mode"

def test_interfaces():
    "Test the xpcom.components.interfaces object"

    iid = xpcom.components.interfaces.nsISupports
    assert iid == xpcom._xpcom.IID_nsISupports, "Got the wrong IID!"
    iid = xpcom.components.interfaces['nsISupports']
    assert iid == xpcom._xpcom.IID_nsISupports, "Got the wrong IID!"
    
    # Test dictionary semantics
    num_fetched = num_nsisupports = 0
    for name, iid in xpcom.components.interfaces.items():
        num_fetched = num_fetched + 1
        if name == "nsISupports":
            num_nsisupports = num_nsisupports + 1
            assert iid == xpcom._xpcom.IID_nsISupports, "Got the wrong IID!"
        assert xpcom.components.interfaces[name] == iid
    # Check all the lengths match.
    assert len(xpcom.components.interfaces.keys()) == len(xpcom.components.interfaces.values()) == \
               len(xpcom.components.interfaces.items()) == len(xpcom.components.interfaces) == \
               num_fetched, "The collection lengths were wrong"
    if num_nsisupports != 1:
        print "Didnt find exactly 1 nsiSupports!"
    print "The interfaces object appeared to work!"

def test_classes():
    # Need a well-known contractID here?
    prog_id = "@mozilla.org/supports-array;1"
    clsid = xpcom.components.ID("{bda17d50-0d6b-11d3-9331-00104ba0fd40}")

    # Check we can create the instance (dont check we can do anything with it tho!)
    klass = xpcom.components.classes[prog_id]
    instance = klass.createInstance()
    
    # Test dictionary semantics
    num_fetched = num_mine = 0
    for name, klass in xpcom.components.classes.items():
        num_fetched = num_fetched + 1
        if name == prog_id:
            if klass.clsid != clsid:
                print "Eeek - didn't get the correct IID - got", klass.clsid
            num_mine = num_mine + 1

# xpcom appears to add charset info to the contractid!?         
#        assert xpcom.components.classes[name].contractid == prog_id, "Expected '%s', got '%s'" % (prog_id, xpcom.components.classes[name].contractid)
    # Check all the lengths match.
    if len(xpcom.components.classes.keys()) == len(xpcom.components.classes.values()) == \
               len(xpcom.components.classes.items()) == len(xpcom.components.classes) == \
               num_fetched:
        pass
    else:
        raise RuntimeError, "The collection lengths were wrong"
    if num_fetched <= 0:
        raise RuntimeError, "Didnt get any classes!!!"
    if num_mine != 1:
        raise RuntimeError, "Didnt find exactly 1 of my contractid! (%d)" % (num_mine,)
    print "The classes object appeared to work!"
    
def test_id():
    id = xpcom.components.ID(str(xpcom._xpcom.IID_nsISupports))
    assert id == xpcom._xpcom.IID_nsISupports
    print "The ID function appeared to work!"
    

# regrtest doesnt like if __name__=='__main__' blocks - it fails when running as a test!
test_interfaces()
test_classes()
test_id()
