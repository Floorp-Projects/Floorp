#!/usr/bin/python -B

from __future__ import print_function
from optparse import OptionParser
import os
import re
import subprocess
import sys

parser = OptionParser()
parser.add_option('--topsrcdir', dest='topsrcdir',
                  help='path to mozilla-central')
parser.add_option('--binjsdir', dest='binjsdir',
                  help='cwd when running binjs_encode')
parser.add_option('--binjs_encode', dest='binjs_encode',
                  help='path to binjs_encode commad')
(options, args) = parser.parse_args()


def ensure_dir(path, name):
    if not os.path.isdir(path):
        print('{} directory {} does not exit'.format(name, path),
              file=sys.stderr)
        sys.exit(1)


def ensure_file(path, name):
    if not os.path.isfile(path):
        print('{} {} does not exit'.format(name, path),
              file=sys.stderr)
        sys.exit(1)


ensure_dir(options.topsrcdir, 'topsrcdir')
ensure_dir(options.binjsdir, 'binjsdir')
ensure_file(options.binjs_encode, 'binjs_encode command')


def encode(infile_path, outfile_path, binjs_encode_args=[]):
    print(infile_path)

    infile = open(infile_path)
    outfile = open(outfile_path, 'w')

    binjs_encode = subprocess.Popen([options.binjs_encode] + binjs_encode_args,
                                    cwd=options.binjsdir,
                                    stdin=infile, stdout=outfile)

    if binjs_encode.wait() != 0:
        print('binjs_encode failed',
              file=sys.stderr)
        sys.exit(1)


def encode_inplace(dir, *args, **kwargs):
    js_pat = re.compile('\.js$')
    for root, dirs, files in os.walk(dir):
        for filename in files:
            if filename.endswith('.js'):
                binjsfilename = js_pat.sub('.binjs', filename)
                infile_path = os.path.join(root, filename)
                outfile_path = os.path.join(root, binjsfilename)
                encode(infile_path, outfile_path, *args, **kwargs)


wpt_dir = os.path.join(options.topsrcdir, 'testing', 'web-platform')
ensure_dir(wpt_dir, 'wpt')

wpt_binast_dir = os.path.join(wpt_dir, 'mozilla', 'tests', 'binast')
ensure_dir(wpt_binast_dir, 'binast in wpt')

encode(os.path.join(wpt_binast_dir, 'large-binjs.js'),
       os.path.join(wpt_binast_dir, 'large.binjs'))
encode(os.path.join(wpt_binast_dir, 'small-binjs.js'),
       os.path.join(wpt_binast_dir, 'small.binjs'))

binast_test_dir = os.path.join(options.topsrcdir, 'js', 'src', 'jsapi-tests', 'binast')
ensure_dir(binast_test_dir, 'binast in jsapi-tests')

encode_inplace(os.path.join(binast_test_dir, 'parser', 'tester'),
               binjs_encode_args=['advanced', 'expanded'])
encode_inplace(os.path.join(binast_test_dir, 'parser', 'unit'),
               binjs_encode_args=['advanced', 'expanded'])
encode_inplace(os.path.join(binast_test_dir, 'parser', 'multipart'))
