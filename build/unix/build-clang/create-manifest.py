#!/bin/python

import os
import os.path
import simplejson
import sys
import subprocess
import urllib
import glob

def check_run(args):
    r = subprocess.call(args)
    assert r == 0

old_files = glob.glob('*.manifest')
for f in old_files:
    try:
        os.unlink(f)
    except:
        pass

basedir = os.path.split(os.path.realpath(sys.argv[0]))[0]
tooltool = basedir + '/tooltool.py'
setup = basedir + '/setup.sh'

check_run(['python', tooltool, '-m', 'linux32.manifest', 'add',
           'clang-linux32.tar.bz2', setup])
check_run(['python', tooltool, '-m', 'linux64.manifest', 'add',
           'clang-linux64.tar.bz2', setup])
check_run(['python', tooltool, '-m', 'darwin.manifest', 'add',
           'clang-darwin.tar.bz2', setup])

def key_sort(item):
    item = item[0]
    if item == 'size':
        return 0
    if item == 'digest':
        return 1
    if item == 'algorithm':
        return 3
    return 4

rev = os.path.basename(os.getcwd()).split('-')[1]

for platform in ['darwin', 'linux32', 'linux64']:
    old_name = 'clang-' + platform + '.tar.bz2'
    manifest = platform + '.manifest'
    data = eval(file(manifest).read())
    new_name = data[1]['digest']
    data[1]['filename'] = 'clang.tar.bz2'
    data = [{'clang_version' : 'r%s' % rev }] + data
    out = file(manifest,'w')
    simplejson.dump(data, out, indent=0, item_sort_key=key_sort)
    out.write('\n')
    os.rename(old_name, new_name)
