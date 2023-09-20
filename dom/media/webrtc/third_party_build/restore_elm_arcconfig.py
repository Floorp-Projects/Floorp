#!/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from subprocess import run

# This script sets the Arcanist configuration for the elm repo. This script
# should be run after each repository reset.
#
# Usage: from the root of the repo `./mach python dom/media/webrtc/third_party_build/restore_elm_arcconfig.py`
#

ret = run(
    [
        "hg",
        "import",
        "-m",
        "Bug 1729988 - FLOAT - REPO-elm - update .arcconfig repo callsign r=bgrins",
        "dom/media/webrtc/third_party_build/elm_arcconfig.patch",
    ]
).returncode
if ret != 0:
    raise Exception(f"Failed to add FLOATing arcconfig patch for ELM: { ret }")
else:
    print("ELM .arcconfig restored. Please push this change to ELM:")
    print("    hg push -r tip")
