#!/usr/bin/env python3

"This tool is intended to be used from meson"

import shutil
import sys

from pathlib import Path

if len (sys.argv) < 3:
	sys.exit (__doc__)

OUTPUT = Path (sys.argv[1])
CURRENT_SOURCE_DIR = Path (sys.argv[2])

# make sure input files are unique
sources = [Path(x) for x in sorted(set(sys.argv[3:]))]

with open (OUTPUT, "wb") as f:
	f.write ("".join ('#include "{}"\n'.format (p.resolve ().relative_to (CURRENT_SOURCE_DIR)) for p in sources if p.suffix == ".cc").encode ())

# copy it also to the source tree, but only if it has changed
baseline = CURRENT_SOURCE_DIR / OUTPUT.name
if baseline.read_bytes() != OUTPUT.read_bytes():
	shutil.copyfile (OUTPUT, baseline)
