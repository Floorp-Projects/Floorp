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
# The Original Code is [Open Source Virtual Machine].
#
# The Initial Developer of the Original Code is
# Adobe System Incorporated.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
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

import sys
import re

_target = re.compile("^--target=(.*)$")
_host = re.compile("^--host=.*$")
_arg = re.compile("^--(enable|disable|with|without)-(\w+)(?:=(.*)|$)$")
_yes = re.compile("^(t|true|yes|y|1)$", re.I)
_no = re.compile("^(f|false|no|n|0)$", re.I)

class Options:
    def __init__(self, argv = sys.argv):
	self._args = {}
	self.target = None
	self.host = None

	for arg in argv[1:]:
	    m = _target.search(arg)
	    if m:
		self.target = m.groups(0)
		continue

	    m = _host.search(arg)
	    if m:
		self.host = m.groups(0)
		continue

	    m = _arg.search(arg)
	    if not m:
		raise Exception("Unrecognized command line parameter: '" + arg + "'")

	    (t, n, v) = m.groups()

	    if type(v) == str:
		if _yes.search(v):
		    v = True
		if _no.search(v):
		    v = False

	    if t == "enable" or t == "with":
		if v:
		    self._args[n] = v
		else:
		    self._args[n] = True

	    elif t == "disable" or t == "without":
		if v:
		    raise Exception("--disable-" + n + " does not take a value.")

		self._args[n] = False

    def getBoolArg(self, name, default=None):
	if not name in self._args:
	    return default

	val = self._args[name]
	del self._args[name]

	if type(val) == bool:
	    return val

	raise Exception("Unrecognized value for option '" + name + "'.")

    def getStringArg(self, name, default=None):
	if not name in self._args:
	    return default

	val = self._args[name]
	del self._args[name]
	return val

    def finish(self):
	if len(self._args):
	    raise Exception("Unrecognized command line parameters: " + \
			    ",".join(self._args.keys()))
