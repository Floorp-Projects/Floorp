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

# Class factory
#
# Hardly worth its own source file!
import xpcom
from xpcom import components, nsError, _xpcom


class Factory:
    _com_interfaces_ = components.interfaces.nsIFactory
    # This will only ever be constructed via other Python code,
    # so we can have ctor args.
    def __init__(self, klass):
        self.klass = klass

    def createInstance(self, outer, iid):
        if outer is not None:
            raise xpcom.ServerException(nsError.NS_ERROR_NO_AGGREGATION)

        if xpcom.verbose:
            print "Python Factory creating", self.klass.__name__
        try:
            return self.klass()
        except:
            # An exception here may not be obvious to the user - none
            # of their code has been called yet.  It can be handy on
            # failure to tell the user what class failed!
            _xpcom.LogWarning("Creation of class '%r' failed!\nException details follow\n" % (self.klass,))
            raise

    def lockServer(self, lock):
        if xpcom.verbose:
            print "Python Factory LockServer called -", lock

