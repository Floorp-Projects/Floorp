# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def test(mod, path, entity = None):
  import re
  # ignore anything but b2g and specific overloads from dom and toolkit
  if mod not in ("netwerk", "dom", "toolkit", "security/manager",
                 "devtools/shared",
                 "mobile",
                 "b2g"):
    return "ignore"

  return "error"
