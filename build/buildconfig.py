# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from mozbuild.base import MozbuildObject

config = MozbuildObject.from_environment()

for var in ('topsrcdir', 'topobjdir', 'defines', 'non_global_defines',
            'substs'):
    value = getattr(config, var)
    setattr(sys.modules[__name__], var, value)

substs = dict(substs)

for var in os.environ:
    if var != 'SHELL' and var in substs:
        substs[var] = os.environ[var]
