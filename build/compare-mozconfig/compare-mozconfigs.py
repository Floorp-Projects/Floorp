#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# originally from https://hg.mozilla.org/build/tools/file/4ab9c1a4e05b/scripts/release/compare-mozconfigs.py

from __future__ import unicode_literals

import logging
import os
import sys
import difflib

FAILURE_CODE = 1
SUCCESS_CODE = 0

log = logging.getLogger(__name__)

class ConfigError(Exception):
    pass

def readConfig(configfile):
    c = {}
    execfile(configfile, c)
    return c['whitelist']

def verify_mozconfigs(mozconfig_pair, nightly_mozconfig_pair, platform,
                      mozconfigWhitelist):
    """Compares mozconfig to nightly_mozconfig and compare to an optional
    whitelist of known differences. mozconfig_pair and nightly_mozconfig_pair
    are pairs containing the mozconfig's identifier and the list of lines in
    the mozconfig."""

    # unpack the pairs to get the names, the names are just for
    # identifying the mozconfigs when logging the error messages
    mozconfig_name, mozconfig_lines = mozconfig_pair
    nightly_mozconfig_name, nightly_mozconfig_lines = nightly_mozconfig_pair

    if not mozconfig_lines or not nightly_mozconfig_lines:
        log.info("Missing mozconfigs to compare for %s" % platform)
        return False

    success = True

    diff_instance = difflib.Differ()
    diff_result = diff_instance.compare(mozconfig_lines, nightly_mozconfig_lines)
    diff_list = list(diff_result)

    for line in diff_list:
        clean_line = line[1:].strip()
        if (line[0] == '-' or line[0] == '+') and len(clean_line) > 1:
            # skip comment lines
            if clean_line.startswith('#'):
                continue
            # compare to whitelist
            message = ""
            if line[0] == '-':
                # handle lines that move around in diff
                if '+' + line[1:] in diff_list:
                    continue
                if platform in mozconfigWhitelist.get('release', {}):
                    if clean_line in \
                            mozconfigWhitelist['release'][platform]:
                        continue
            elif line[0] == '+':
                if '-' + line[1:] in diff_list:
                    continue
                if platform in mozconfigWhitelist.get('nightly', {}):
                    if clean_line in \
                            mozconfigWhitelist['nightly'][platform]:
                        continue
                    else:
                        log.warning("%s not in %s %s!" % (
                            clean_line, platform,
                            mozconfigWhitelist['nightly'][platform]))
            else:
                log.error("Skipping line %s!" % line)
                continue
            message = "found in %s but not in %s: %s"
            if line[0] == '-':
                log.error(message % (mozconfig_name,
                                     nightly_mozconfig_name, clean_line))
            else:
                log.error(message % (nightly_mozconfig_name,
                                     mozconfig_name, clean_line))
            success = False
    return success

def get_mozconfig(path):
    """Consumes a path and returns a list of lines from the mozconfig file."""
    with open(path, 'rb') as fh:
        return fh.readlines()

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('topsrcdir', help='Path to root of source checkout')
    parser.add_argument('args', nargs='*')

    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    whitelist = os.path.join(args.topsrcdir, 'browser', 'config', 'mozconfigs',
                             'whitelist')

    mozconfig_whitelist = readConfig(whitelist)

    for arg in args.args:
        platform, mozconfig_path, nightly_mozconfig_path = arg.split(',')

        mozconfig_lines = get_mozconfig(mozconfig_path)
        nightly_mozconfig_lines = get_mozconfig(nightly_mozconfig_path)

        mozconfig_pair = (mozconfig_path, mozconfig_lines)
        nightly_mozconfig_pair = (nightly_mozconfig_path,
                                  nightly_mozconfig_lines)

        passed = verify_mozconfigs(mozconfig_pair, nightly_mozconfig_pair,
                                   platform, mozconfig_whitelist)

        if passed:
            logging.info('Mozconfig check passed!')
        else:
            logging.error('Mozconfig check failed!')
            sys.exit(FAILURE_CODE)
    sys.exit(SUCCESS_CODE)
