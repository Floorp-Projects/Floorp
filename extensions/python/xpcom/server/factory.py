# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

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

