# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to capture the content of config.status-generated
# files and subsequently restore their timestamp if they haven't changed.

import argparse
import errno
import os
import subprocess
import sys
import pickle

import mozpack.path as mozpath


CONFIGURE_DATA = 'configure.pkl'


def prepare(srcdir, objdir, args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache-file', type=str)

    data_file = os.path.join(objdir, CONFIGURE_DATA)
    previous_args = None
    if os.path.exists(data_file):
        with open(data_file, 'rb') as f:
            data = pickle.load(f)
            previous_args = data['args']

    args, others = parser.parse_known_args(args)

    data = {
        'args': others,
        'srcdir': srcdir,
        'objdir': objdir,
        'env': os.environ,
    }

    if args.cache_file:
        data['cache-file'] = mozpath.normpath(mozpath.join(os.getcwd(),
                                                           args.cache_file))
    else:
        data['cache-file'] = mozpath.join(objdir, 'config.cache')

    if previous_args is not None:
        data['previous-args'] = previous_args

    try:
        os.makedirs(objdir)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    with open(data_file, 'wb') as f:
        pickle.dump(data, f)

    return data
