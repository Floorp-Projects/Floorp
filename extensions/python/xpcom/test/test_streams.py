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
# Portions created by the Initial Developer are Copyright (C) 2000,2001
# the Initial Developer. All Rights Reserved.
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

import xpcom
from xpcom import _xpcom, components, COMException, ServerException, nsError
from StringIO import StringIO

test_data = "abcdefeghijklmnopqrstuvwxyz"

class koTestSimpleStreamBase:
    _com_interfaces_ = [components.interfaces.nsIInputStream]
    # We avoid registering this object - see comments in get_test_inout_? below.

    def __init__(self):
        self.data=StringIO(test_data)

    def close( self ):
        pass

    def available( self ):
        return self.data.len-self.data.pos

    def readStr( self, amount):
        return self.data.read(amount)

    read=readStr

    def get_observer( self ):
        raise ServerException(nsError.NS_ERROR_NOT_IMPLEMENTED)

    def set_observer( self, param0 ):
        raise ServerException(nsError.NS_ERROR_NOT_IMPLEMENTED)

# This class has "nonBlocking" as an attribute.
class koTestSimpleStream1(koTestSimpleStreamBase):
    nonBlocking=0

# This class has "nonBlocking" as getter/setters.
class koTestSimpleStream2(koTestSimpleStreamBase):
    def __init__(self):
        koTestSimpleStreamBase.__init__(self)
        self.isNonBlocking = 0
    def get_nonBlocking(self):
        return self.isNonBlocking

def get_test_input_1():
    # We use a couple of internal hacks here that mean we can avoid having the object
    # registered.  This code means that we are still working over the xpcom boundaries, tho
    # (and the point of this test is not the registration, etc).
    import xpcom.server, xpcom.client
    ob = xpcom.server.WrapObject( koTestSimpleStream1(), _xpcom.IID_nsISupports)
    ob = xpcom.client.Component(ob, components.interfaces.nsIInputStream)
    return ob

def get_test_input_2():
    # We use a couple of internal hacks here that mean we can avoid having the object
    # registered.  This code means that we are still working over the xpcom boundaries, tho
    # (and the point of this test is not the registration, etc).
    import xpcom.server, xpcom.client
    ob = xpcom.server.WrapObject( koTestSimpleStream2(), _xpcom.IID_nsISupports)
    ob = xpcom.client.Component(ob, components.interfaces.nsIInputStream)
    return ob

def test_input(myStream):
    if myStream.read(5) != test_data[:5]:
        raise "Read the wrong data!"
    if myStream.read(0) != '':
        raise "Read the wrong emtpy data!"
    if myStream.read(5) != test_data[5:10]:
        raise "Read the wrong data after an empty read!"
    if myStream.read(-1) != test_data[10:]:
        raise "Couldnt read the rest of the data"
    if myStream.nonBlocking:
        raise "Expected default to be blocking"
    try:
        myStream.observer = None
        raise "Shouldnt get here!"
    except COMException, details:
        if details.errno != nsError.NS_ERROR_NOT_IMPLEMENTED:
            raise "Unexpected COM exception: %s (%r)" % (details, details)

if __name__=='__main__':
    test_input( get_test_input_1() )
    test_input( get_test_input_2() )
    print "The stream tests worked!"

