#!/usr/bin/env python

from __future__ import print_function

import io, os, re, sys

headers_content = []
for h in os.environ["headers"].split (' '):
	if h.endswith (".h"):
		with io.open(h, encoding='utf8') as f: headers_content.append (f.read ())

result = ("EXPORTS\n" +
	"\n".join (sorted (re.findall (r"^hb_\w+(?= \()", "\n".join (headers_content), re.M))) +
	"\nLIBRARY libharfbuzz-0.dll")

with open (sys.argv[1], "w") as f: f.write (result)
