# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from mozbuild.backend.configenvironment import PartialConfigEnvironment
from mozbuild.base import MozbuildObject

config = MozbuildObject.from_environment()
partial_config = PartialConfigEnvironment(config.topobjdir)

for var in ("topsrcdir", "topobjdir"):
    value = getattr(config, var)
    setattr(sys.modules[__name__], var, value)

for var in ("defines", "substs", "get_dependencies"):
    value = getattr(partial_config, var)
    setattr(sys.modules[__name__], var, value)
