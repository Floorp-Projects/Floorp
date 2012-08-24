# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import string

propList = eval(sys.stdin.read())
props = ""
for [prop, pref] in propList:
    pref = '[Pref=%s] ' % pref if pref is not "" else ""
    if not prop.startswith("Moz"):
        prop = prop[0].lower() + prop[1:]
    # Unfortunately, even some of the getters here are fallible
    # (e.g. on nsComputedDOMStyle).
    props += "  %sattribute DOMString %s;\n" % (pref, prop)

idlFile = open(sys.argv[1], "r");
idlTemplate = idlFile.read();
idlFile.close();

print string.Template(idlTemplate).substitute({ "props": props })
