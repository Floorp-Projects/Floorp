# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os
import sys

path = os.path.dirname(__file__)
while not os.path.exists(os.path.join(path, 'config.status')):
    parent = os.path.normpath(os.path.join(path, os.pardir))
    if parent == path:
        raise Exception("Can't find config.status")
    path = parent

path = os.path.join(path, 'config.status')
config = imp.load_module('_buildconfig', open(path), path, ('', 'r', imp.PY_SOURCE))

# Copy values from the config.status namespace into this module namespace.
# This effectively imports topsrcdir, topobjdir, defines, substs, files,
# headers and non_global_defines
for var in config.__all__:
    value = getattr(config, var)
    if isinstance(value, list) and value and isinstance(value[0], tuple):
        value = dict(value)
    setattr(sys.modules[__name__], var, value)

for var in os.environ:
    if var != 'SHELL' and var in substs:
        substs[var] = os.environ[var]
