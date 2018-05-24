#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# originally from https://hg.mozilla.org/build/tools/file/4ab9c1a4e05b/scripts/release/compare-mozconfigs.py  # NOQA: E501

from __future__ import unicode_literals

import logging
import os
import difflib
import unittest

import buildconfig
import mozunit

FAILURE_CODE = 1
SUCCESS_CODE = 0

PLATFORMS = (
    'linux32',
    'linux64',
    'macosx64',
    'win32',
    'win64',
)

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
    diff_result = diff_instance.compare(
        mozconfig_lines, nightly_mozconfig_lines)
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


def compare(topsrcdir):
    app = os.path.join(topsrcdir, 'browser')
    whitelist = readConfig(os.path.join(app, 'config', 'mozconfigs',
                                        'whitelist'))

    success = True

    def normalize_lines(lines):
        return {l.strip() for l in lines}

    for platform in PLATFORMS:
        log.info('Comparing platform %s' % platform)

        mozconfigs_path = os.path.join(app, 'config', 'mozconfigs', platform)

        nightly_path = os.path.join(mozconfigs_path, 'nightly')
        beta_path = os.path.join(mozconfigs_path, 'beta')
        release_path = os.path.join(mozconfigs_path, 'release')

        nightly_lines = get_mozconfig(nightly_path)
        beta_lines = get_mozconfig(beta_path)
        release_lines = get_mozconfig(release_path)

        # Validate that entries in whitelist['nightly'][platform] are actually
        # present.
        whitelist_normalized = normalize_lines(
            whitelist['nightly'].get(platform, []))
        nightly_normalized = normalize_lines(nightly_lines)

        for line in sorted(whitelist_normalized - nightly_normalized):
            log.error('extra line in nightly whitelist: %s' % line)
            success = False

        log.info('Comparing beta and nightly mozconfigs')
        passed = verify_mozconfigs((beta_path, beta_lines),
                                   (nightly_path, nightly_lines),
                                   platform,
                                   whitelist)

        if not passed:
            success = False

        log.info('Comparing release and nightly mozconfigs')
        passed = verify_mozconfigs((release_path, release_lines),
                                   (nightly_path, nightly_lines),
                                   platform,
                                   whitelist)
        if not passed:
            success = False

    return success


class TestCompareMozconfigs(unittest.TestCase):
    def test_compare_mozconfigs(self):
        topsrcdir = buildconfig.substs['top_srcdir']
        self.assertTrue(compare(topsrcdir))


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    mozunit.main()
