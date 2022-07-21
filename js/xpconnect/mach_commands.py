# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys
from pathlib import Path

from mach.decorators import (
    Command,
    CommandArgument,
)


@Command("xpcshell", category="misc", description="Run the xpcshell binary")
@CommandArgument(
    "args", nargs=argparse.REMAINDER, help="Arguments to provide to xpcshell"
)
def xpcshell(command_context, args):
    dist_bin = Path(command_context._topobjdir, "dist", "bin")
    browser_dir = dist_bin / "browser"

    if sys.platform == "win32":
        xpcshell = dist_bin / "xpcshell.exe"
    else:
        xpcshell = dist_bin / "xpcshell"

    command = [
        str(xpcshell),
        "-g",
        str(dist_bin),
        "-a",
        str(browser_dir),
    ]

    if args:
        command.extend(args)

    return command_context.run_process(
        command,
        pass_thru=True,
        ensure_exit_code=False,
    )
