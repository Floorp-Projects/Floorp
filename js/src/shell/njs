#!/usr/bin/python
#
# Narcissus 'shell' for use with jstests.py
# Expects to be in the same directory as ./js
# Expects the Narcissus src files to be in ./narcissus/

import os, re, sys
from subprocess import *
from optparse import OptionParser

THIS_DIR = os.path.dirname(__file__)
NARC_JS_DIR = os.path.abspath(os.path.join(THIS_DIR, 'narcissus'))

js_cmd = os.path.abspath(os.path.join(THIS_DIR, "js"))

narc_jsdefs = os.path.join(NARC_JS_DIR, "jsdefs.js")
narc_jslex = os.path.join(NARC_JS_DIR, "jslex.js")
narc_jsparse = os.path.join(NARC_JS_DIR, "jsparse.js")
narc_jsexec = os.path.join(NARC_JS_DIR, "jsexec.js")


if __name__ == '__main__':
    op = OptionParser(usage='%prog [TEST-SPECS]')
    op.add_option('-f', '--file', dest='js_files', action='append',
            help='JS file to load', metavar='FILE')
    op.add_option('-e', '--expression', dest='js_exps', action='append',
            help='JS expression to evaluate')

    (options, args) = op.parse_args()

    cmd = 'evaluate("__NARCISSUS__=true;"); '
    if options.js_exps:
        for exp in options.js_exps:
            cmd += 'evaluate("%s"); ' % exp.replace('"', '\\"')

    if options.js_files:
        for file in options.js_files:
            cmd += 'evaluate(snarf("%(file)s"), "%(file)s", 1); ' % {'file':file }

    Popen([js_cmd, '-f', narc_jsdefs, '-f', narc_jslex, '-f', narc_jsparse, '-f', narc_jsexec, '-e', cmd]).wait()

