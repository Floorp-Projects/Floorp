#!/usr/bin/env python3

import sys, os, re

os.chdir (os.environ.get ('srcdir', os.path.dirname (__file__)))

HBHEADERS = os.environ.get ('HBHEADERS', '').split () or \
	[x for x in os.listdir ('.') if x.startswith ('hb') and x.endswith ('.h')]
HBSOURCES = os.environ.get ('HBSOURCES', '').split () or \
	[x for x in os.listdir ('.') if x.startswith ('hb') and x.endswith (('.cc', '.hh'))]

stat = 0

for x in HBHEADERS + HBSOURCES:
	if not x.endswith ('h') or x == 'hb-gobject-structs.h': continue
	tag = x.upper ().replace ('.', '_').replace ('-', '_')
	with open (x, 'r', encoding='utf-8') as f: content = f.read ()
	if len (re.findall (tag + r'\b', content)) != 3:
		print ('Ouch, header file %s does not have correct preprocessor guards' % x)
		stat = 1

sys.exit (stat)
