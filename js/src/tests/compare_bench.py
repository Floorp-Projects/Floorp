#!/usr/bin/env python2.4
"""usage: %progname candidate_path baseline_path
"""

from __future__ import print_function

import json
import optparse
from operator import itemgetter


def avg(seq):
    return sum(seq) / len(seq)


def compare(current, baseline):
    percent_speedups = []
    for key, current_result in current.iteritems():
        try:
            baseline_result = baseline[key]
        except KeyError:
            print(key, 'missing from baseline')
            continue
        val_getter = itemgetter('average_ms', 'stddev_ms')
        base_avg, base_stddev = val_getter(baseline_result)
        current_avg, current_stddev = val_getter(current_result)
        t_best = current_avg - current_stddev
        t_worst = current_avg + current_stddev
        base_t_best = base_avg - base_stddev
        base_t_worst = base_avg + base_stddev
        if t_worst < base_t_best:
            # Worst takes less time (better) than baseline's best.
            speedup = -((t_worst - base_t_best) / base_t_best) * 100
            result = 'faster: {:6.2f}ms < baseline {:6.2f}ms ({:+6.2f}%)'.format(
                t_worst, base_t_best, speedup)
            percent_speedups.append(speedup)
        elif t_best > base_t_worst:
            # Best takes more time (worse) than baseline's worst.
            slowdown = -((t_best - base_t_worst) / base_t_worst) * 100
            result = 'SLOWER: {:6.2f}ms > baseline {:6.2f}ms ({:+6.2f}%) '.format(
                t_best, base_t_worst, slowdown)
            percent_speedups.append(slowdown)
        else:
            result = 'Meh.'
        print('{:30s}: {}'.format(key, result))
    if percent_speedups:
        print('Average speedup: {:.2f}%'.format(avg(percent_speedups)))


def compare_immediate(current_map, baseline_path):
    baseline_file = open(baseline_path)
    baseline_map = json.load(baseline_file)
    baseline_file.close()
    compare(current_map, baseline_map)


def main(candidate_path, baseline_path):
    candidate_file, baseline_file = open(candidate_path), open(baseline_path)
    candidate = json.load(candidate_file)
    baseline = json.load(baseline_file)
    compare(candidate, baseline)
    candidate_file.close()
    baseline_file.close()


if __name__ == '__main__':
    parser = optparse.OptionParser(usage=__doc__.strip())
    options, args = parser.parse_args()
    try:
        candidate_path = args.pop(0)
    except IndexError:
        parser.error('A JSON filepath to compare against baseline is required')
    try:
        baseline_path = args.pop(0)
    except IndexError:
        parser.error('A JSON filepath for baseline is required')
    main(candidate_path, baseline_path)
