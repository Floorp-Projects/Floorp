#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import json
import argparse
import sys
import shutil
from functools import reduce


def check_run(args, path):
    print(' '.join(args) + ' in ' + path, file=sys.stderr)
    subprocess.run(args, cwd=path, check=True)


def run_in(path, args, extra_env=None):
    '''
    Runs the given commands in the directory specified by <path>.
    '''
    env = dict(os.environ)
    env.update(extra_env or {})
    check_run(args, path)
    subprocess.run(args, cwd=path)


def build_tar_package(tar, name, base, directories):
    name = os.path.realpath(name)
    run_in(base, [tar,
                  '-c',
                  '-%s' % ('J' if '.xz' in name else 'j'),
                  '-f',
                  name] + directories)


def is_git_repo(dir):
    '''Check whether the given directory is a git repository.'''
    from subprocess import CalledProcessError
    try:
        check_run(['git', 'rev-parse'], dir)
        return True
    except CalledProcessError:
        return False


def git_clone(main_dir, url, clone_dir, commit):
    '''
    Clones the repository from <url> into <clone_dir>, and brings the
    repository to the state of <commit>.
    '''
    run_in(main_dir, ['git', 'clone', url, clone_dir])
    run_in(clone_dir, ['git', 'checkout', commit])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', required=True,
                        type=argparse.FileType('r'),
                        help='Infer configuration file')
    parser.add_argument('-b', '--base-dir',
                        help="Base directory for code and build artifacts")
    parser.add_argument('--clean', action='store_true',
                        help='Clean the build directory')
    parser.add_argument('--skip-tar', action='store_true',
                        help='Skip tar packaging stage')

    args = parser.parse_args()

    # The directories end up in the debug info, so the easy way of getting
    # a reproducible build is to run it in a know absolute directory.
    # We use a directory that is registered as a volume in the Docker image.
    if args.base_dir:
        base_dir = args.base_dir
    else:
        base_dir = reduce(os.path.join,
                          [os.sep + 'builds', 'worker',
                           'workspace', 'moz-toolchain'])
    infer_dir = os.path.join(base_dir, 'infer')
    source_dir = os.path.join(infer_dir, 'src')
    build_dir = os.path.join(infer_dir, 'build')

    if args.clean:
        shutil.rmtree(build_dir)
        os.sys.exit(0)

    config = json.load(args.config)
    infer_revision = config['infer_revision']
    infer_repo = config['infer_repo']

    for folder in [infer_dir, source_dir, build_dir]:
        os.makedirs(folder, exist_ok=True)

    # clone infer
    if not is_git_repo(source_dir):
        # git doesn't like cloning into a non-empty folder. If src is not a git
        # repo then just remove it in order to reclone
        shutil.rmtree(source_dir)
        git_clone(infer_dir, infer_repo, source_dir, infer_revision)
    # apply a few patches
    dir_path = os.path.dirname(os.path.realpath(__file__))
    # clean the git directory by reseting all changes
    git_commands = [['clean', '-f'], ['reset', '--hard']]
    for command in git_commands:
        run_in(source_dir, ['git']+command)
    for p in config.get('patches', []):
        run_in(source_dir, ['git', 'apply', os.path.join(dir_path, p)])
    # build infer
    run_in(source_dir, ['./build-infer.sh', 'java'],
           extra_env={'NO_CMAKE_STRIP': '1'})
    package_name = 'infer'
    if not args.skip_tar:
        build_tar_package('tar', '%s.tar.xz' % (package_name),
                          source_dir, [os.path.join('infer', 'bin'),
                                       os.path.join('infer', 'lib')])
