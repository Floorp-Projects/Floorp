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
