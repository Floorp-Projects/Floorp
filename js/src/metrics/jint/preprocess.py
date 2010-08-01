# vim: set ts=4 sw=4 tw=99 et: 

import os, re
import tempfile
import subprocess
import sys

def realpath(k):
    return os.path.realpath(os.path.normpath(k))

class Benchmark:
    def __init__(self, JS, fname):
        self.fname = fname
        self.JS = JS
        self.stats = { }
        self.runList = [ ]

    def run(self, name, eargs):
        args = [self.JS]
        args.extend(eargs)
        args.append(bm.fd.name)
        output = subprocess.check_output(args).decode()
        self.stats[name] = { }
        self.runList.append(name)
        for line in output.split('\n'):
            m = re.search('line (\d+): (\d+)', line)
            if m:
                self.stats[name][int(m.group(1))] = int(m.group(2))
            else:
                m = re.search('total: (\d+)', line)
                if m:
                    self.stats[name]['total'] = m.group(1)

    def winnerForLine(self, line):
        best = self.runList[0]
        bestTime = self.stats[best][line]
        for run in self.runList[1:]:
            x = self.stats[run][line]
            if x < bestTime:
                best = run
                bestTime = x
        return best

    def chart(self):
        sys.stdout.write('{0:7s}'.format(''))
        sys.stdout.write('{0:15s}'.format('line'))
        for run in self.runList:
            sys.stdout.write('{0:15s}'.format(run))
        sys.stdout.write('{0:15s}\n'.format('best'))
        for c in self.counters:
            sys.stdout.write('{0:10d}'.format(c))
            for run in self.runList:
                sys.stdout.write('{0:15d}'.format(self.stats[run][c]))
            sys.stdout.write('{0:12s}'.format(''))
            sys.stdout.write('{0:15s}'.format(self.winnerForLine(c)))
            sys.stdout.write('\n')

    def preprocess(self):
        rd = open(self.fname, 'rt')
        stack = []
        lineno = 1
        lines = []
        self.counters = []
        try:
            for line in rd:
                lines.append(line)
                if re.search('\/\* BEGIN LOOP \*\/', line):
                    stack.append(lineno)
                    self.counters.append(lineno)
                    lines.append('JINT_TRACKER.line_' + str(lineno) + '_start = Date.now();\n')
                elif re.search('\/\* END LOOP \*\/', line):
                    old = stack.pop()
                    lines.append('JINT_TRACKER.line_' + str(old) + '_end = Date.now();\n')
                    lines.append('JINT_TRACKER.line_' + str(old) + '_total += ' + \
                                 'JINT_TRACKER.line_' + str(old) + '_end - ' + \
                                 'JINT_TRACKER.line_' + str(old) + '_start;\n')
                lineno += 1
            fd = tempfile.NamedTemporaryFile('wt')
            fd.write('var JINT_TRACKER = { };\n')
            for c in self.counters:
                fd.write('JINT_TRACKER.line_' + str(c) + '_start = 0;\n')
                fd.write('JINT_TRACKER.line_' + str(c) + '_end = 0;\n')
                fd.write('JINT_TRACKER.line_' + str(c) + '_total = 0;\n')
            fd.write('JINT_TRACKER.begin = Date.now();\n')
            for line in lines:
                fd.write(line)
            fd.write('JINT_TRACKER.total = Date.now() - JINT_TRACKER.begin;\n')
            for c in self.counters:
                fd.write('print("line ' + str(c) + ': " + JINT_TRACKER.line_' + str(c) +
                               '_total);')
            fd.write('print("total: " + JINT_TRACKER.total);')
            fd.flush()
            self.fd = fd
        except:
            raise

if __name__ == '__main__':
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    test_dir = os.path.join(script_dir, 'tests')
    lib_dir = os.path.join(script_dir, 'lib')

    # The [TESTS] optional arguments are paths of test files relative
    # to the trace-test/tests directory.

    from optparse import OptionParser
    op = OptionParser(usage='%prog [options] JS_SHELL test')
    (OPTIONS, args) = op.parse_args()
    if len(args) < 2:
        op.error('missing JS_SHELL and test argument')
    # We need to make sure we are using backslashes on Windows.
    JS = realpath(args[0])
    test = realpath(args[1])

    bm = Benchmark(JS, test)
    bm.preprocess()
    bm.run('mjit', ['-m'])
    bm.run('tjit', ['-j'])
    bm.run('m+tjit', ['-m', '-j'])
    bm.chart()

