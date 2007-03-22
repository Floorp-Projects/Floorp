# Utilities for registering functions to be called at xpcom shutdown.
#
# Pass xpcom.shutdown.register a function (and optionally args) that should
# be called as xpcom shutsdown.  Relies on xpcom itself sending the
# standard shutdown notification.

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
# The Initial Developer of the Original Code is Mark Hammond.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

import xpcom.server
from xpcom import _xpcom
from xpcom.components import interfaces

import logging

_handlers = []

class _ShutdownObserver:
    _com_interfaces_ = interfaces.nsIObserver
    def observe(self, service, topic, extra):
        logger = logging.getLogger('xpcom')
        while _handlers:
            func, args, kw = _handlers.pop()
            try:
                logger.debug("Calling shutdown handler '%s'(*%s, **%s)",
                             func, args, kw)
                func(*args, **kw)
            except:
                logger.exception("Shutdown handler '%s' failed", func)

def register(func, *args, **kw):
    _handlers.append( (func, args, kw) )

# Register
svc = _xpcom.GetServiceManager().getServiceByContractID(
                                    "@mozilla.org/observer-service;1",
                                    interfaces.nsIObserverService)

svc.addObserver(_ShutdownObserver(), "xpcom-shutdown", 0)

del svc, _ShutdownObserver
