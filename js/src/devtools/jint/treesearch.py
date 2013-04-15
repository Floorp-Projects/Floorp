# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# vim: set ts=4 sw=4 tw=99 et: 

import os, re
import tempfile
import subprocess
import sys, math
import datetime
import random

def realpath(k):
    return os.path.realpath(os.path.normpath(k))

class UCTNode:
    def __init__(self, loop):
        self.children = None
        self.loop = loop
        self.visits = 1
        self.score = 0

    def addChild(self, child):
        if self.children == None:
            self.children = []
        self.children.append(child)

    def computeUCB(self, coeff):
        return (self.score / self.visits) + math.sqrt(coeff / self.visits)

class UCT:
    def __init__(self, benchmark, bestTime, enableLoops, loops, fd, playouts):
        self.bm = benchmark
        self.fd = fd
        self.numPlayouts = playouts
        self.maxNodes = self.numPlayouts * 20
        self.loops = loops
        self.enableLoops = enableLoops
        self.maturityThreshold = 20
        self.originalBest = bestTime
        self.bestTime = bestTime
        self.bias = 20
        self.combos = []
        self.zobrist = { }
        random.seed()

    def expandNode(self, node, pending):
        for loop in pending:
            node.addChild(UCTNode(loop))
            self.numNodes += 1
            if self.numNodes >= self.maxNodes:
                return False
        return True

    def findBestChild(self, node):
        coeff = self.bias * math.log(node.visits)
        bestChild = None
        bestUCB = -float('Infinity')

        for child in node.children:
            ucb = child.computeUCB(coeff)
            if ucb >= bestUCB:
                bestUCB = ucb
                bestChild = child

        return child

    def playout(self, history):
        queue = []
        for i in range(0, len(self.loops)):
            queue.append(random.randint(0, 1))
        for node in history:
            queue[node.loop] = not self.enableLoops
        zash = 0
        for i in range(0, len(queue)):
            if queue[i]:
                zash |= (1 << i)
        if zash in self.zobrist:
            return self.zobrist[zash]

        self.bm.generateBanList(self.loops, queue)
        result = self.bm.treeSearchRun(self.fd, ['-m', '-j'], 3)
        self.zobrist[zash] = result
        return result

    def step(self, loopList):
        node = self.root
        pending = loopList[:]
        history = [node]

        while True:
            # If this is a leaf node...
            if node.children == None:
                # And the leaf node is mature...
                if node.visits >= self.maturityThreshold:
                    # If the node can be expanded, keep spinning.
                    if self.expandNode(node, pending) and node.children != None:
                        continue

                # Otherwise, this is a leaf node. Run a playout.
                score = self.playout(history)
                break

            # Find the best child.
            node = self.findBestChild(node)
            history.append(node)
            pending.remove(node.loop)

        # Normalize the score.
        origScore = score
        score = (self.originalBest - score) / self.originalBest

        for node in history:
            node.visits += 1
            node.score += score

        if int(origScore) < int(self.bestTime):
            print('New best score: {0:f}ms'.format(origScore))
            self.combos = [history]
            self.bestTime = origScore
        elif int(origScore) == int(self.bestTime):
            self.combos.append(history)

    def run(self):
        loopList = [i for i in range(0, len(self.loops))]
        self.numNodes = 1
        self.root = UCTNode(-1)
        self.expandNode(self.root, loopList)

        for i in range(0, self.numPlayouts):
            self.step(loopList)

        # Build the expected combination vector.
        combos = [ ]
        for combo in self.combos:
            vec = [ ]
            for i in range(0, len(self.loops)):
                vec.append(int(self.enableLoops))
            for node in combo:
                vec[node.loop] = int(not self.enableLoops)
            combos.append(vec)

        return [self.bestTime, combos]

class Benchmark:
    def __init__(self, JS, fname):
        self.fname = fname
        self.JS = JS
        self.stats = { }
        self.runList = [ ]

    def run(self, fd, eargs):
        args = [self.JS]
        args.extend(eargs)
        args.append(fd.name)
        return subprocess.check_output(args).decode()

    #    self.stats[name] = { }
    #    self.runList.append(name)
    #    for line in output.split('\n'):
    #        m = re.search('line (\d+): (\d+)', line)
    #        if m:
    #            self.stats[name][int(m.group(1))] = int(m.group(2))
    #        else:
    #            m = re.search('total: (\d+)', line)
    #            if m:
    #                self.stats[name]['total'] = m.group(1)

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

    def preprocess(self, lines, onBegin, onEnd):
        stack = []
        counters = []
        rd = open(self.fname, 'rt')
        for line in rd:
            if re.search('\/\* BEGIN LOOP \*\/', line):
                stack.append([len(lines), len(counters)])
                counters.append([len(lines), 0])
                onBegin(lines, len(lines))
            elif re.search('\/\* END LOOP \*\/', line):
                old = stack.pop()
                onEnd(lines, old[0], len(lines))
                counters[old[1]][1] = len(lines)
            else:
                lines.append(line)
        return [lines, counters]

    def treeSearchRun(self, fd, args, count = 5):
        total = 0
        for i in range(0, count):
            output = self.run(fd, args)
            total += int(output)
        return total / count

    def generateBanList(self, counters, queue):
        if os.path.exists('/tmp/permabans'):
            os.unlink('/tmp/permabans')
        fd = open('/tmp/permabans', 'wt')
        for i in range(0, len(counters)):
            for j in range(counters[i][0], counters[i][1] + 1):
                fd.write('{0:d} {1:d}\n'.format(j, int(queue[i])))
        fd.close()

    def internalExhaustiveSearch(self, params):
        counters = params['counters']

        # iterative algorithm to explore every combination
        ncombos = 2 ** len(counters)
        queue = []
        for c in counters:
            queue.append(0)

        fd = params['fd']
        bestTime = float('Infinity')
        bestCombos = []

        i = 0
        while i < ncombos:
            temp = i
            for j in range(0, len(counters)):
                queue[j] = temp & 1
                temp = temp >> 1
            self.generateBanList(counters, queue)

            t = self.treeSearchRun(fd, ['-m', '-j'])
            if (t < bestTime):
                bestTime = t
                bestCombos = [queue[:]]
                print('New best time: {0:f}ms'.format(t))
            elif int(t) == int(bestTime):
                bestCombos.append(queue[:])

            i = i + 1

        return [bestTime, bestCombos]

    def internalTreeSearch(self, params):
        fd = params['fd']
        methodTime = params['methodTime']
        tracerTime = params['tracerTime']
        combinedTime = params['combinedTime']
        counters = params['counters']

        # Build the initial loop data.
        # If the method JIT already wins, disable tracing by default.
        # Otherwise, enable tracing by default.
        if methodTime < combinedTime:
            enableLoops = True
        else:
            enableLoops = False

        enableLoops = False

        uct = UCT(self, combinedTime, enableLoops, counters[:], fd, 50000)
        return uct.run()

    def treeSearch(self):
        fd, counters = self.ppForTreeSearch()

        os.system("cat " + fd.name + " > /tmp/k.js")

        if os.path.exists('/tmp/permabans'):
            os.unlink('/tmp/permabans')
        methodTime = self.treeSearchRun(fd, ['-m'])
        tracerTime = self.treeSearchRun(fd, ['-j'])
        combinedTime = self.treeSearchRun(fd, ['-m', '-j'])

        #Get a rough estimate of how long this benchmark will take to fully compute.
        upperBound = max(methodTime, tracerTime, combinedTime)
        upperBound *= 2 ** len(counters)
        upperBound *= 5    # Number of runs
        treeSearch = False
        if (upperBound < 1000):
            print('Estimating {0:d}ms to test, so picking exhaustive '.format(int(upperBound)) +
                  'search.')
        else:
            upperBound = int(upperBound / 1000)
            delta = datetime.timedelta(seconds = upperBound)
            if upperBound < 180:
                print('Estimating {0:d}s to test, so picking exhaustive '.format(int(upperBound)))
            else:
                print('Estimating {0:s} to test, so picking tree search '.format(str(delta)))
                treeSearch = True

        best = min(methodTime, tracerTime, combinedTime)

        params = {
                    'fd': fd,
                    'counters': counters,
                    'methodTime': methodTime,
                    'tracerTime': tracerTime,
                    'combinedTime': combinedTime
                 }

        print('Method JIT:  {0:d}ms'.format(int(methodTime)))
        print('Tracing JIT: {0:d}ms'.format(int(tracerTime)))
        print('Combined:    {0:d}ms'.format(int(combinedTime)))

        if 1 and treeSearch:
            results = self.internalTreeSearch(params)
        else:
            results = self.internalExhaustiveSearch(params)

        bestTime = results[0]
        bestCombos = results[1]
        print('Search found winning time {0:d}ms!'.format(int(bestTime)))
        print('Combos at this time: {0:d}'.format(len(bestCombos)))

        #Find loops that traced every single time
        for i in range(0, len(counters)):
            start = counters[i][0]
            end = counters[i][1]
            n = len(bestCombos)
            for j in bestCombos:
                n -= j[i]
            print('\tloop @ {0:d}-{1:d} traced {2:d}% of the time'.format(
                    start, end, int(n / len(bestCombos) * 100)))

    def ppForTreeSearch(self):
        def onBegin(lines, lineno):
            lines.append('GLOBAL_THINGY = 1;\n')
        def onEnd(lines, old, lineno):
            lines.append('GLOBAL_THINGY = 1;\n')

        lines = ['var JINT_START_TIME = Date.now();\n',
                 'var GLOBAL_THINGY = 0;\n']

        lines, counters = self.preprocess(lines, onBegin, onEnd)
        fd = tempfile.NamedTemporaryFile('wt')
        for line in lines:
            fd.write(line)
        fd.write('print(Date.now() - JINT_START_TIME);\n')
        fd.flush()
        return [fd, counters]

    def preprocessForLoopCounting(self):
        def onBegin(lines, lineno):
            lines.append('JINT_TRACKER.line_' + str(lineno) + '_start = Date.now();\n')

        def onEnd(lines, old, lineno):
            lines.append('JINT_TRACKER.line_' + str(old) + '_end = Date.now();\n')
            lines.append('JINT_TRACKER.line_' + str(old) + '_total += ' + \
                         'JINT_TRACKER.line_' + str(old) + '_end - ' + \
                         'JINT_TRACKER.line_' + str(old) + '_start;\n')

        lines, counters = self.preprocess(onBegin, onEnd)
        fd = tempfile.NamedTemporaryFile('wt')
        fd.write('var JINT_TRACKER = { };\n')
        for c in counters:
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
        return fd

if __name__ == '__main__':
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    test_dir = os.path.join(script_dir, 'tests')
    lib_dir = os.path.join(script_dir, 'lib')

    # The [TESTS] optional arguments are paths of test files relative
    # to the jit-test/tests directory.

    from optparse import OptionParser
    op = OptionParser(usage='%prog [options] JS_SHELL test')
    (OPTIONS, args) = op.parse_args()
    if len(args) < 2:
        op.error('missing JS_SHELL and test argument')
    # We need to make sure we are using backslashes on Windows.
    JS = realpath(args[0])
    test = realpath(args[1])

    bm = Benchmark(JS, test)
    bm.treeSearch()
    # bm.preprocess()
    # bm.run('mjit', ['-m'])
    # bm.run('tjit', ['-j'])
    # bm.run('m+tjit', ['-m', '-j'])
    # bm.chart()

