# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

try:
    # Python 3.2
    from tempfile import TemporaryDirectory
except ImportError:
    import tempfile
    import shutil
    from contextlib import contextmanager

    @contextmanager
    def TemporaryDirectory(*args, **kwds):
        d = tempfile.mkdtemp(*args, **kwds)
        try:
            yield d
        finally:
            shutil.rmtree(d)
