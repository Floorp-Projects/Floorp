#!/usr/bin/python

#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the static rooting analysis
"""

from subprocess import Popen
import subprocess
import os
import argparse
import sys

def env(config):
    e = os.environ
    e['PATH'] = '%s:%s/bin' % (e['PATH'], config['sixgill'])
    e['XDB'] = '%(sixgill)s/bin/xdb.so' % config
    e['SOURCE_ROOT'] = config['source'] or e['TARGET']
    return e

def fill(command, config):
    try:
        return tuple(s % config for s in command)
    except:
        print("Substitution failed:")
        for fragment in command:
            try:
                fragment % config
            except:
                print("  %s" % fragment)
        raise Hell

def print_command(command, outfile=None, env=None):
    output = ' '.join(command)
    if outfile:
        output += ' > ' + outfile
    if env:
        changed = {}
        e = os.environ
        for key,value in env.items():
            if (key not in e) or (e[key] != value):
                changed[key] = value
        if changed:
            output = ' '.join(key + "='" + value + "'" for key, value in changed.items()) + ' ' + output
    print output

def generate_hazards(config, outfilename):
    jobs = []
    for i in range(config['jobs']):
        command = fill(('%(js)s',
                        '%(analysis_scriptdir)s/analyzeRoots.js',
                        '%(gcFunctions_list)s',
                        '%(suppressedFunctions_list)s',
                        '%(gcTypes)s',
                        str(i+1), '%(jobs)s',
                        'tmp.%s' % (i+1,)),
                       config)
        outfile = 'rootingHazards.%s' % (i+1,)
        output = open(outfile, 'w')
        print_command(command, outfile=outfile, env=env(config))
        jobs.append((command, Popen(command, stdout=output, env=env(config))))

    final_status = 0
    while jobs:
        pid, status = os.wait()
        jobs = [ job for job in jobs if job[1].pid != pid ]
        final_status = final_status or status

    if final_status:
        raise subprocess.CalledProcessError(final_status, 'analyzeRoots.js')

    with open(outfilename, 'w') as output:
        command = ['cat'] + [ 'rootingHazards.%s' % (i+1,) for i in range(config['jobs']) ]
        print_command(command, outfile=outfilename)
        subprocess.call(command, stdout=output)

JOBS = { 'dbs':
             (('%(CWD)s/run_complete',
               '--foreground',
               '--build-root=%(objdir)s',
               '--work=dir=work',
               '-b', '%(sixgill)s/bin',
               '--buildcommand=%(buildcommand)s',
               '.'),
              None),

         'callgraph':
             (('%(js)s', '%(analysis_scriptdir)s/computeCallgraph.js'),
              'callgraph.txt'),

         'gcFunctions':
             (('%(js)s', '%(analysis_scriptdir)s/computeGCFunctions.js', '%(callgraph)s'),
              'gcFunctions.txt'),

         'gcFunctions_list':
             (('perl', '-lne', 'print $1 if /^GC Function: (.*)/', '%(gcFunctions)s'),
              'gcFunctions.lst'),

         'suppressedFunctions_list':
             (('perl', '-lne', 'print $1 if /^Suppressed Function: (.*)/', '%(gcFunctions)s'),
              'suppressedFunctions.lst'),

         'gcTypes':
             (('%(js)s', '%(analysis_scriptdir)s/computeGCTypes.js',),
              'gcTypes.txt'),

         'allFunctions':
             (('%(sixgill)s/bin/xdbkeys', 'src_body.xdb',),
              'allFunctions.txt'),

         'hazards':
             (generate_hazards, 'rootingHazards.txt')
         }

def run_job(name, config):
    command, outfilename = JOBS[name]
    print("Running " + name + " to generate " + str(outfilename))
    if hasattr(command, '__call__'):
        command(config, outfilename)
    else:
        command = fill(command, config)
        temp = '%s.tmp' % name
        print_command(command, outfile=outfilename, env=env(config))
        with open(temp, 'w') as output:
            subprocess.check_call(command, stdout=output, env=env(config))
        if outfilename is not None:
            os.rename(temp, outfilename)

config = { 'CWD': os.path.dirname(__file__) }

defaults = [ '%s/defaults.py' % config['CWD'] ]

for default in defaults:
    try:
        execfile(default, config)
    except:
        pass

data = config.copy()

parser = argparse.ArgumentParser(description='Statically analyze build tree for rooting hazards.')
parser.add_argument('target', metavar='TARGET', type=str, nargs='?',
                    help='run starting from this target')
parser.add_argument('--source', metavar='SOURCE', type=str, nargs='?',
                    help='source code to analyze')
parser.add_argument('--jobs', '-j', default=4, metavar='JOBS', type=int,
                    help='number of simultaneous analyzeRoots.js jobs')
parser.add_argument('--list', const=True, nargs='?', type=bool,
                    help='display available targets')
parser.add_argument('--buildcommand', '--build', '-b', type=str, nargs='?',
                    help='command to build the tree being analyzed')
parser.add_argument('--tag', '-t', type=str, nargs='?',
                    help='name of job, also sets build command to "build.<tag>"')

args = parser.parse_args()
data.update(vars(args))

if args.tag and not args.buildcommand:
    args.buildcommand="build.%s" % args.tag

if args.buildcommand:
    data['buildcommand'] = args.buildcommand
elif 'BUILD' in os.environ:
    data['buildcommand'] = os.environ['BUILD']
else:
    data['buildcommand'] = 'make -j4 -s'

targets = [ 'dbs',
            'callgraph',
            'gcTypes',
            'gcFunctions',
            'gcFunctions_list',
            'suppressedFunctions_list',
            'allFunctions',
            'hazards' ]

if args.list:
    for target in targets:
        command, outfilename = JOBS[target]
        if outfilename:
            print("%s -> %s" % (target, outfilename))
        else:
            print(target)
    sys.exit(0)

for target in targets:
    command, outfilename = JOBS[target]
    data[target] = outfilename

if args.target:
    targets = targets[targets.index(args.target):]

for target in targets:
    run_job(target, data)
