#!/usr/bin/env python3

"usage: gen-hb-version.py 1.0.0 hb-version.h.in hb-version.h"

import os, re, sys

os.chdir (os.path.dirname (__file__))

if len (sys.argv) < 4:
	sys.exit(__doc__)

version = sys.argv[1]
major, minor, micro = version.split (".")
input = sys.argv[2]
output = sys.argv[3]

with open (input, "r", encoding='utf-8') as input_file:
	generated = (input_file.read ()
		.replace ("@HB_VERSION_MAJOR@", major)
		.replace ("@HB_VERSION_MINOR@", minor)
		.replace ("@HB_VERSION_MICRO@", micro)
		.replace ("@HB_VERSION@", version)
		.encode ())

	with open (output, "rb") as current_file:
		current = current_file.read()

	# write only if is changed
	if generated != current:
		with open (output, "wb") as output_file:
			output_file.write (generated)
