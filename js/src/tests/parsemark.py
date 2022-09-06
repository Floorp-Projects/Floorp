#!/usr/bin/env python

"""%prog [options] shellpath dirpath

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
  shellpath             executable JavaScript shell
  dirpath               directory filled with parsilicious js files
"""

from __future__ import print_function

import math
import optparse
import os
import subprocess as subp
import sys
import json
from string import Template

try:
    import compare_bench
except ImportError:
    compare_bench = None


_DIR = os.path.dirname(__file__)
JS_CODE_TEMPLATE = Template(
    """
if (typeof snarf !== 'undefined') read = snarf
var contents = read("$filepath");
$prepare
for (var i = 0; i < $warmup_run_count; i++)
    $func(contents, $options);
var results = [];
for (var i = 0; i < $real_run_count; i++) {
    var start = elapsed() / 1000;
    $func(contents, $options);
    var end = elapsed() / 1000;
    results.push(end - start);
}
print(results);
"""
)


def gen_filepaths(dirpath, target_ext=".js"):
    for filename in os.listdir(dirpath):
        if filename.endswith(target_ext):
            yield os.path.join(dirpath, filename)


def avg(seq):
    return sum(seq) / len(seq)


def stddev(seq, mean):
    diffs = ((float(item) - mean) ** 2 for item in seq)
    return math.sqrt(sum(diffs) / len(seq))


def bench(
    shellpath, filepath, warmup_runs, counted_runs, prepare, func, options, stfu=False
):
    """Return a list of milliseconds for the counted runs."""
    assert '"' not in filepath
    code = JS_CODE_TEMPLATE.substitute(
        filepath=filepath,
        warmup_run_count=warmup_runs,
        real_run_count=counted_runs,
        prepare=prepare,
        func=func,
        options=options,
    )
    proc = subp.Popen([shellpath, "-e", code], stdout=subp.PIPE)
    stdout, _ = proc.communicate()
    milliseconds = [float(val) for val in stdout.decode().split(",")]
    mean = avg(milliseconds)
    sigma = stddev(milliseconds, mean)
    if not stfu:
        print("Runs:", [int(ms) for ms in milliseconds])
        print("Mean:", mean)
        print("Stddev: {:.2f} ({:.2f}% of mean)".format(sigma, sigma / mean * 100))
    return mean, sigma


def parsemark(filepaths, fbench, stfu=False):
    """:param fbench: fbench(filename) -> float"""
    bench_map = {}  # {filename: (avg, stddev)}
    for filepath in filepaths:
        filename = os.path.split(filepath)[-1]
        if not stfu:
            print("Parsemarking {}...".format(filename))
        bench_map[filename] = fbench(filepath)
    print("{")
    for i, (filename, (avg, stddev)) in enumerate(iter(bench_map.items())):
        assert '"' not in filename
        fmt = '    {:30s}: {{"average_ms": {:6.2f}, "stddev_ms": {:6.2f}}}'
        if i != len(bench_map) - 1:
            fmt += ","
        filename_str = '"{}"'.format(filename)
        print(fmt.format(filename_str, avg, stddev))
    print("}")
    return dict(
        (filename, dict(average_ms=avg, stddev_ms=stddev))
        for filename, (avg, stddev) in iter(bench_map.items())
    )


def main():
    parser = optparse.OptionParser(usage=__doc__.strip())
    parser.add_option(
        "-w",
        "--warmup-runs",
        metavar="COUNT",
        type=int,
        default=5,
        help="used to minimize test instability [%default]",
    )
    parser.add_option(
        "-c",
        "--counted-runs",
        metavar="COUNT",
        type=int,
        default=50,
        help="timed data runs that count towards the average" " [%default]",
    )
    parser.add_option(
        "-s",
        "--shell",
        metavar="PATH",
        help="explicit shell location; when omitted, will look" " in likely places",
    )
    parser.add_option(
        "-b",
        "--baseline",
        metavar="JSON_PATH",
        dest="baseline_path",
        help="json file with baseline values to " "compare against",
    )
    parser.add_option(
        "--mode",
        dest="mode",
        type="choice",
        choices=("parse", "dumpStencil", "compile", "decode"),
        default="parse",
        help="The target of the benchmark (parse/dumpStencil/compile/decode), defaults to parse",
    )
    parser.add_option(
        "--lazy",
        dest="lazy",
        action="store_true",
        default=False,
        help="Use lazy parsing when compiling",
    )
    parser.add_option(
        "-q",
        "--quiet",
        dest="stfu",
        action="store_true",
        default=False,
        help="only print JSON to stdout [%default]",
    )
    options, args = parser.parse_args()
    try:
        shellpath = args.pop(0)
    except IndexError:
        parser.print_help()
        print()
        print("error: shellpath required", file=sys.stderr)
        return -1
    try:
        dirpath = args.pop(0)
    except IndexError:
        parser.print_help()
        print()
        print("error: dirpath required", file=sys.stderr)
        return -1
    if not shellpath or not os.path.exists(shellpath):
        print("error: could not find shell:", shellpath, file=sys.stderr)
        return -1
    if options.baseline_path:
        if not os.path.isfile(options.baseline_path):
            print("error: baseline file does not exist", file=sys.stderr)
            return -1
        if not compare_bench:
            print(
                "error: JSON support is missing, cannot compare benchmarks",
                file=sys.stderr,
            )
            return -1

    if options.lazy and options.mode == "parse":
        print(
            "error: parse mode doesn't support lazy",
            file=sys.stderr,
        )
        return -1

    funcOpt = {}
    if options.mode == "decode":
        encodeOpt = {}
        encodeOpt["execute"] = False
        encodeOpt["saveIncrementalBytecode"] = True
        if not options.lazy:
            encodeOpt["forceFullParse"] = True

        # In order to test the decoding, we first have to encode the content.
        prepare = Template(
            """
contents = cacheEntry(contents);
evaluate(contents, $options);
"""
        ).substitute(options=json.dumps(encodeOpt))

        func = "evaluate"
        funcOpt["execute"] = False
        funcOpt["loadBytecode"] = True
        if not options.lazy:
            funcOpt["forceFullParse"] = True
    else:
        prepare = ""
        func = options.mode
        if not options.lazy:
            funcOpt["forceFullParse"] = True

    def benchfile(filepath):
        return bench(
            shellpath,
            filepath,
            options.warmup_runs,
            options.counted_runs,
            prepare,
            func,
            json.dumps(funcOpt),
            stfu=options.stfu,
        )

    bench_map = parsemark(gen_filepaths(dirpath), benchfile, options.stfu)
    if options.baseline_path:
        compare_bench.compare_immediate(bench_map, options.baseline_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
