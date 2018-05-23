#! /usr/bin/env python3
assert __name__ != '__main__'

'''
Any time we vendor[1] from an external git repo, we want to keep a record of the csets
we're pulling from.

This script leaves a record of the merge-base reference tip and cherry-picks that we pull
into Gecko. (such as gfx/angle/cherry_picks.txt)
'''

from pathlib import *
import subprocess
import sys

# --

def print_now(*args):
    print(*args)
    sys.stdout.flush()


def run_checked(*args, **kwargs):
    print_now(' ', args)
    return subprocess.run(args, check=True, **kwargs)

# --

def record_cherry_picks(dir_in_gecko, merge_base_from):
    assert '/' in merge_base_from, 'Please specify a reference tip from a remote.'
    log_path = Path(dir_in_gecko, 'cherry_picks.txt')
    print_now('Logging cherry picks to {}.'.format(log_path))

    print('cwd:', Path.cwd())
    merge_base = run_checked('git', 'merge-base', 'HEAD', merge_base_from,
                             stdout=subprocess.PIPE).stdout.decode().strip()

    mb_info = run_checked('git', 'log', '{}~1..{}'.format(merge_base, merge_base),
                          stdout=subprocess.PIPE).stdout
    cherries = run_checked('git', 'log', merge_base + '..', stdout=subprocess.PIPE).stdout

    with open(log_path, 'wb') as f:
        f.write(cherries)
        f.write(b'\nCherries picked')
        f.write(b'\n' + (b'=' * 80))
        f.write(b'\nMerge base from: ' + merge_base_from.encode())
        f.write(b'\n\n')
        f.write(mb_info)
