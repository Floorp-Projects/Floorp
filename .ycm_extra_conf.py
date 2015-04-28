# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os
from StringIO import StringIO
import shlex

path = os.path.join(os.path.dirname(__file__), 'mach')

if not os.path.exists(path):
    path = os.path.join(os.path.dirname(__file__), 'config.status')
    config = imp.load_module('_buildconfig', open(path), path, ('', 'r', imp.PY_SOURCE))
    path = os.path.join(config.topsrcdir, 'mach')
mach_module = imp.load_module('_mach', open(path), path, ('', 'r', imp.PY_SOURCE))

def FlagsForFile(filename):
    mach = mach_module.get_mach()
    out = StringIO()
    out.encoding = None
    mach.run(['compileflags', filename], stdout=out, stderr=out)

    return {
        'flags': shlex.split(out.getvalue()),
        'do_cache': True
    }
