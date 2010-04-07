#!/usr/bin/env python

"""%prog [options] dirpath

Pulls performance data on parsing via the js shell.
Displays the average number of milliseconds it took to parse each file.

For comparison, something apparently approximating a t-test is performed:
"Faster" means that:

    t_baseline_goodrun = (t_baseline_avg - t_baseline_stddev)
    t_current_badrun = (t_current_avg + t_current_stddev) 
    t_current_badrun < t_baseline_goodrun

Effectively, a bad run from the current data is better than a good run from the
baseline data, we're probably faster. A similar computation is used for
determining the "slower" designation.

Arguments:
  dirpath               directory filled with parsilicious js files
"""

import math
import optparse
import os
import subprocess as subp
import sys
from string import Template
from operator import itemgetter


_DIR = os.path.dirname(__file__)
JS_CODE_TEMPLATE = Template("""
var contents = snarf("$filepath");
for (let i = 0; i < $warmup_run_count; i++)
    compile(contents);
var results = [];
for (let i = 0; i < $real_run_count; i++) {
    var start = new Date();
    compile(contents);
    var end = new Date();
    results.push(end - start);
}
print(results);
""")


def find_shell(filename='js'):
    """Look around for the js shell. Prefer more obvious places to look.
    
    :return: Path if found, else None.
    """
    relpaths = ['', 'obj', os.pardir, [os.pardir, 'obj']]
    for relpath in relpaths:
        path_parts = [_DIR]
        if isinstance(relpath, list):
            path_parts += relpath
        else:
            path_parts.append(relpath)
        path_parts.append(filename)
        path = os.path.join(*path_parts)
        if os.path.isfile(path):
            return path


def gen_filepaths(dirpath, target_ext='.js'):
    for filename in os.listdir(dirpath):
        if filename.endswith(target_ext):
            yield os.path.join(dirpath, filename)


def avg(seq):
    return sum(seq) / len(seq)


def stddev(seq, mean):
    diffs = ((float(item) - mean) ** 2 for item in seq)
    return math.sqrt(sum(diffs) / len(seq))


def bench(shellpath, filepath, warmup_runs, counted_runs, stfu=False):
    """Return a list of milliseconds for the counted runs."""
    assert '"' not in filepath
    code = JS_CODE_TEMPLATE.substitute(filepath=filepath,
            warmup_run_count=warmup_runs, real_run_count=counted_runs)
    proc = subp.Popen([shellpath, '-e', code], stdout=subp.PIPE)
    stdout, _ = proc.communicate()
    milliseconds = [float(val) for val in stdout.split(',')]
    mean = avg(milliseconds)
    sigma = stddev(milliseconds, mean)
    if not stfu:
        print 'Runs:', milliseconds
        print 'Mean:', mean
        print 'Stddev: %.2f (%.2f%% of mean)' % (sigma, sigma / mean * 100)
    return mean, sigma


def parsemark(filepaths, fbench, stfu=False):
    """:param fbench: fbench(filename) -> float"""
    bench_map = {}
    for filepath in filepaths:
        filename = os.path.split(filepath)[-1]
        if not stfu:
            print 'Parsemarking %s...' % filename
        bench_map[filename] = fbench(filepath)
    print '{'
    for i, (filename, (avg, stddev)) in enumerate(bench_map.iteritems()):
        assert '"' not in filename
        fmt = '    %30s: {"average_ms": %6.2f, "stddev_ms": %6.2f}'
        if i != len(bench_map) - 1:
            fmt += ','
        filename_str = '"%s"' % filename
        print fmt % (filename_str, avg, stddev)
    print '}'
    return bench_map


def compare(current, baseline):
    for key, (avg, stddev) in current.iteritems():
        try:
            base_avg, base_stddev = itemgetter('average_ms', 'stddev_ms')(baseline.get(key, None))
        except TypeError:
            print key, 'missing from baseline'
            continue
        t_best, t_worst = avg - stddev, avg + stddev
        base_t_best, base_t_worst = base_avg - base_stddev, base_avg + base_stddev
        fmt = '%30s: %s'
        if t_worst < base_t_best: # Worst takes less time (better) than baseline's best.
            speedup = -((t_worst - base_t_best) / base_t_best) * 100
            result = 'faster: %6.2fms < baseline %6.2fms (%+6.2f%%)' % \
                    (t_worst, base_t_best, speedup)
        elif t_best > base_t_worst: # Best takes more time (worse) than baseline's worst.
            slowdown = -((t_best - base_t_worst) / base_t_worst) * 100
            result = 'SLOWER: %6.2fms > baseline %6.2fms (%+6.2f%%) ' % \
                    (t_best, base_t_worst, slowdown)
        else:
            result = 'Meh.'
        print '%30s: %s' % (key, result)


def try_import_json():
    try:
        import json
        return json
    except ImportError:
        try:
            import simplejson as json
            return json
        except ImportError:
            pass


def main():
    parser = optparse.OptionParser(usage=__doc__.strip())
    parser.add_option('-w', '--warmup-runs', metavar='COUNT', type=int,
            default=5, help='used to minimize test instability')
    parser.add_option('-c', '--counted-runs', metavar='COUNT', type=int,
            default=20, help='timed data runs that count towards the average')
    parser.add_option('-s', '--shell', metavar='PATH', help='explicit shell '
            'location; when omitted, will look in likely places')
    parser.add_option('-b', '--baseline', metavar='JSON_PATH',
            dest='baseline_path', help='json file with baseline values to '
            'compare against')
    parser.add_option('-q', '--quiet', dest='stfu', action='store_true',
            default=False, help='only print JSON to stdout')
    options, args = parser.parse_args()
    try:
        dirpath = args.pop(0)
    except IndexError:
        parser.print_help()
        print
        print >> sys.stderr, 'error: dirpath required'
        return -1
    shellpath = options.shell or find_shell()
    if not shellpath:
        print >> sys.stderr, 'Could not find shell'
        return -1
    if options.baseline_path:
        if not os.path.isfile(options.baseline_path):
            print >> sys.stderr, 'Baseline file does not exist'
            return -1
        json = try_import_json()
        if not json:
            print >> sys.stderr, 'You need a json lib for baseline comparison'
            return -1
    benchfile = lambda filepath: bench(shellpath, filepath,
            options.warmup_runs, options.counted_runs, stfu=options.stfu)
    bench_map = parsemark(gen_filepaths(dirpath), benchfile, options.stfu)
    if options.baseline_path:
        fh = open(options.baseline_path, 'r') # 2.4 compat, no 'with'.
        baseline_map = json.load(fh)
        fh.close()
        compare(current=bench_map, baseline=baseline_map)
    return 0


if __name__ == '__main__':
    sys.exit(main())
