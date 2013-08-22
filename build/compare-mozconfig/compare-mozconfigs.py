#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# originally from http://hg.mozilla.org/build/tools/file/4ab9c1a4e05b/scripts/release/compare-mozconfigs.py

from __future__ import unicode_literals

import logging
import os
import site
import sys
import urllib2
import difflib

FAILURE_CODE = 1
SUCCESS_CODE = 0

log = logging.getLogger(__name__)

class ConfigError(Exception):
    pass

def make_hg_url(hgHost, repoPath, protocol='https', revision=None,
                filename=None):
    """construct a valid hg url from a base hg url (hg.mozilla.org),
    repoPath, revision and possible filename"""
    base = '%s://%s' % (protocol, hgHost)
    repo = '/'.join(p.strip('/') for p in [base, repoPath])
    if not filename:
        if not revision:
            return repo
        else:
            return '/'.join([p.strip('/') for p in [repo, 'rev', revision]])
    else:
        assert revision
        return '/'.join([p.strip('/') for p in [repo, 'raw-file', revision,
                         filename]])

def readConfig(configfile, keys=[], required=[]):
    c = {}
    execfile(configfile, c)
    for k in keys:
        c = c[k]
    items = c.keys()
    err = False
    for key in required:
        if key not in items:
            err = True
            log.error("Required item `%s' missing from %s" % (key, c))
    if err:
        raise ConfigError("Missing at least one item in config, see above")
    return c

def verify_mozconfigs(mozconfig_pair, nightly_mozconfig_pair, platform,
                      mozconfigWhitelist={}):
    """Compares mozconfig to nightly_mozconfig and compare to an optional
    whitelist of known differences. mozconfig_pair and nightly_mozconfig_pair
    are pairs containing the mozconfig's identifier and the list of lines in
    the mozconfig."""

    # unpack the pairs to get the names, the names are just for
    # identifying the mozconfigs when logging the error messages
    mozconfig_name, mozconfig_lines = mozconfig_pair
    nightly_mozconfig_name, nightly_mozconfig_lines = nightly_mozconfig_pair

    missing_args = mozconfig_lines == [] or nightly_mozconfig_lines == []
    if missing_args:
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
                if platform in mozconfigWhitelist.get('release', {}):
                    if clean_line in \
                            mozconfigWhitelist['release'][platform]:
                        continue
            elif line[0] == '+':
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

def get_mozconfig(path, options):
    """Consumes a path and returns a list of lines from
    the mozconfig file. If download is required, the path
    specified should be relative to the root of the hg
    repository e.g browser/config/mozconfigs/linux32/nightly"""
    if options.no_download:
        return open(path, 'r').readlines()
    else:
        url = make_hg_url(options.hghost, options.branch, 'http',
                    options.revision, path)
        return urllib2.urlopen(url).readlines()

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()

    parser.add_option('--branch', dest='branch')
    parser.add_option('--revision', dest='revision')
    parser.add_option('--hghost', dest='hghost', default='hg.mozilla.org')
    parser.add_option('--whitelist', dest='whitelist')
    parser.add_option('--no-download', action='store_true', dest='no_download',
                      default=False)
    options, args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    missing_args = options.branch is None or options.revision is None
    if not options.no_download and missing_args:
        logging.error('Not enough arguments to download mozconfigs')
        sys.exit(FAILURE_CODE)

    mozconfig_whitelist = readConfig(options.whitelist, ['whitelist'])

    for arg in args:
        platform, mozconfig_path, nightly_mozconfig_path = arg.split(',')

        mozconfig_lines = get_mozconfig(mozconfig_path, options)
        nightly_mozconfig_lines = get_mozconfig(nightly_mozconfig_path, options)

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
