# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import zlib


# The built-in hash over the str type is non-deterministic, so we need to do
# this instead.
def hash_str(s):
    assert isinstance(s, str)
    return zlib.adler32(s.encode("utf-8"))
