#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import re
import json
import subprocess

testdir = os.path.abspath(os.path.dirname(__file__))

cfg = {}
cfg['SIXGILL_ROOT']   = os.environ.get('SIXGILL',
                                       os.path.join(testdir, "sixgill"))
cfg['SIXGILL_BIN']    = os.environ.get('SIXGILL_BIN',
                                       os.path.join(cfg['SIXGILL_ROOT'], "usr", "bin"))
cfg['SIXGILL_PLUGIN'] = os.environ.get('SIXGILL_PLUGIN',
                                       os.path.join(cfg['SIXGILL_ROOT'], "usr", "libexec", "sixgill", "gcc", "xgill.so"))
cfg['CC']             = os.environ.get("CC",
                                       "gcc")
cfg['CXX']            = os.environ.get("CXX",
                                       cfg.get('CC', 'g++'))
cfg['JS_BIN']         = os.environ["JS"]

def binpath(prog):
    return os.path.join(cfg['SIXGILL_BIN'], prog)

if not os.path.exists("test-output"):
    os.mkdir("test-output")

# Simplified version of the body info.
class Body(dict):
    def __init__(self, body):
        self['BlockIdKind'] = body['BlockId']['Kind']
        if 'Variable' in body['BlockId']:
            self['BlockName'] = body['BlockId']['Variable']['Name'][0]
        self['LineRange'] = [ body['Location'][0]['Line'], body['Location'][1]['Line'] ]
        self['Filename'] = body['Location'][0]['CacheString']
        self['Edges'] = body.get('PEdge', [])
        self['Points'] = { i+1: body['PPoint'][i]['Location']['Line'] for i in range(len(body['PPoint'])) }
        self['Index'] = body['Index']
        self['Variables'] = { x['Variable']['Name'][0]: x['Type'] for x in body['DefineVariable'] }

        # Indexes
        self['Line2Points'] = {}
        for point, line in self['Points'].items():
            self['Line2Points'].setdefault(line, []).append(point)
        self['SrcPoint2Edges'] = {}
        for edge in self['Edges']:
            (src, dst) = edge['Index']
            self['SrcPoint2Edges'].setdefault(src, []).append(edge)
        self['Line2Edges'] = {}
        for (src, edges) in self['SrcPoint2Edges'].items():
            line = self['Points'][src]
            self['Line2Edges'].setdefault(line, []).extend(edges)

    def edges_from_line(self, line):
        return self['Line2Edges'][line]

    def edge_from_line(self, line):
        edges = self.edges_from_line(line)
        assert(len(edges) == 1)
        return edges[0]

    def edges_from_point(self, point):
        return self['SrcPoint2Edges'][point]

    def edge_from_point(self, point):
        edges = self.edges_from_point(point)
        assert(len(edges) == 1)
        return edges[0]

    def assignment_point(self, varname):
        for edge in self['Edges']:
            if edge['Kind'] != 'Assign':
                continue
            dst = edge['Exp'][0]
            if dst['Kind'] != 'Var':
                continue
            if dst['Variable']['Name'][0] == varname:
                return edge['Index'][0]
        raise Exception("assignment to variable %s not found" % varname)

    def assignment_line(self, varname):
        return self['Points'][self.assignment_point(varname)]

tests = ['test']
for name in tests:
    indir = os.path.join(testdir, name)
    outdir = os.path.join(testdir, "test-output", name)
    if not os.path.exists(outdir):
        os.mkdir(outdir)

    def compile(source):
        cmd = "{CXX} -c {source} -fplugin={sixgill}".format(source=os.path.join(indir, source),
                                                            CXX=cfg['CXX'], sixgill=cfg['SIXGILL_PLUGIN'])
        print("Running %s" % cmd)
        subprocess.check_call(["sh", "-c", cmd])

    def load_db_entry(dbname, pattern):
        if not isinstance(pattern, basestring):
            output = subprocess.check_output([binpath("xdbkeys"), dbname + ".xdb"])
            entries = output.splitlines()
            matches = [f for f in entries if re.search(pattern, f)]
            if len(matches) == 0:
                raise Exception("entry not found")
            if len(matches) > 1:
                raise Exception("multiple entries found")
            pattern = matches[0]

        output = subprocess.check_output([binpath("xdbfind"), "-json", dbname + ".xdb", pattern])
        return json.loads(output)

    def computeGCTypes():
        file("defaults.py", "w").write('''\
analysis_scriptdir = '{testdir}'
sixgill_bin = '{bindir}'
'''.format(testdir=testdir, bindir=cfg['SIXGILL_BIN']))
        cmd = [
            os.path.join(testdir, "analyze.py"),
            "gcTypes", "--upto", "gcTypes",
            "--source=%s" % indir,
            "--objdir=%s" % outdir,
            "--js=%s" % cfg['JS_BIN'],
        ]
        print("Running " + " ".join(cmd))
        output = subprocess.check_call(cmd)

    def loadGCTypes():
        gctypes = {'GCThings': [], 'GCPointers': []}
        for line in file(os.path.join(outdir, "gcTypes.txt")):
            m = re.match(r'^(GC\w+): (.*)', line)
            if m:
                gctypes[m.group(1) + 's'].append(m.group(2))
        return gctypes

    def process_body(body):
        return Body(body)

    def process_bodies(bodies):
        return [ process_body(b) for b in bodies ]

    def equal(got, expected):
        if got != expected:
            print("Got '%s', expected '%s'" % (got, expected))

    os.chdir(outdir)
    subprocess.call(["sh", "-c", "rm *.xdb"])
    execfile(os.path.join(indir, "test.py"))
    print("TEST-PASSED: %s" % name)
