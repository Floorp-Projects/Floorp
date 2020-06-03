#!/usr/bin/env python3

import os, re, sys, subprocess, shutil, tempfile

os.chdir (os.path.dirname (__file__))

if len (sys.argv) < 2:
	ragel_sources = [x for x in os.listdir ('.') if x.endswith ('.rl')]
else:
	ragel_sources = sys.argv[2:]

ragel = shutil.which ('ragel')

if not ragel:
	print ('You have to install ragel if you are going to develop HarfBuzz itself')
	exit (1)

if not len (ragel_sources):
	exit (77)

tempdir = tempfile.mkdtemp ()

for rl in ragel_sources:
	hh = rl.replace ('.rl', '.hh')
	shutil.copy (rl, tempdir)
	# writing to stdout has some complication on Windows
	subprocess.Popen ([ragel, '-e', '-F1', '-o', hh, rl], cwd=tempdir).wait ()

	generated_path = os.path.join (tempdir, hh)
	with open (generated_path, "rb") as temp_file:
		generated = temp_file.read()

	with open (hh, "rb") as current_file:
		current = current_file.read()

	# overwrite only if is changed
	if generated != current:
		shutil.copyfile (generated_path, hh)

shutil.rmtree (tempdir)
