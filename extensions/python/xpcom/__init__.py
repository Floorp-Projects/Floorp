# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

# The XPCOM (Cross Platform COM) package.
import exceptions

#import sys
#sys.stdout = open("pystdout.log", "w")

# A global "verbose" flag - currently used by the
# server package to print trace messages
verbose = 0
hr_map = {}

# The standard XPCOM exception object.
# Instances of this class are raised by the XPCOM extension module.
class Exception(exceptions.Exception):
    def __init__(self, errno, message = None):
        assert int(errno) == errno, "The errno param must be an integer"
        self.errno = errno
        self.message = message
        exceptions.Exception.__init__(self, errno)
    def __str__(self):
        if not hr_map:
            import nsError
            for name, val in nsError.__dict__.items():
                if type(val)==type(0):
                    hr_map[val] = name
        message = self.message
        if message is None:
            message = hr_map.get(self.errno)
            if message is None:
                message = ""
        return "0x%x (%s)" % (self.errno, message)

# An alias for Exception - allows code to say "from xpcom import COMException"
# rather than "Exception" - thereby preventing clashes.
COMException = Exception

# Exceptions thrown by servers.  It can be good for diagnostics to
# differentiate between a ServerException (which was presumably explicitly thrown)
# and a normal exception which may simply be propagating down.
# (When ServerException objects are thrown across the XPConnect
# gateway they will be converted back to normal client exceptions if
# subsequently re-caught by Python
class ServerException(Exception):
    def __init__(self, errno=None, *args, **kw):
        if errno is None:
            import nsError
            errno = nsError.NS_ERROR_FAILURE
        Exception.__init__(self, errno, *args, **kw)

# Some global functions.
