#!/usr/bin/python
import logging
import os
import site
import sys
import urllib2

site.addsitedir(os.path.join(os.path.dirname(__file__), "../../lib/python"))

from release.sanity import verify_mozconfigs
from release.info import readConfig
from util.hg import make_hg_url

FAILURE_CODE = 1
SUCCESS_CODE = 0

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
