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
# Activestate Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <MarkH@activestate.com>
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
#

# The XPCOM (Cross Platform COM) package.
import exceptions


import sys
#sys.stdout = open("pystdout.log", "w")
if sys.version_info >= (2, 3):
    # sick off the new hex() warnings, and no time to digest what the 
    # impact will be!
    import warnings
    warnings.filterwarnings("ignore", category=FutureWarning, append=1)

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
