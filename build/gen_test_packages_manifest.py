#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

from argparse import ArgumentParser

ALL_HARNESSES = [
    'common', # Harnesses without a specific package will look here.
    'mochitest',
    'reftest',
    'webapprt',
    'xpcshell',
    'cppunittest',
    'jittest',
    'mozbase',
    'web-platform',
    'talos',
    'gtest',
]

PACKAGE_SPECIFIED_HARNESSES = [
    'cppunittest',
    'mochitest',
    'reftest',
    'xpcshell',
    'web-platform',
    'talos',
]

# These packages are not present for every build configuration.
OPTIONAL_PACKAGES = [
    'gtest',
]


def parse_args():
    parser = ArgumentParser(description='Generate a test_packages.json file to tell automation which harnesses require which test packages.')
    parser.add_argument("--common", required=True,
                        action="store", dest="tests_common",
                        help="Name of the \"common\" archive, a package to be used by all harnesses.")
    parser.add_argument("--jsshell", required=True,
                        action="store", dest="jsshell",
                        help="Name of the jsshell zip.")
    parser.add_argument("--use-short-names", action="store_true",
                        help="Use short names for packages (target.$name.tests.zip "
                             "instead of $(PACKAGE_BASENAME).$name.tests.zip)")
    for harness in PACKAGE_SPECIFIED_HARNESSES:
        parser.add_argument("--%s" % harness, required=True,
                            action="store", dest=harness,
                            help="Name of the %s zip." % harness)
    for harness in OPTIONAL_PACKAGES:
        parser.add_argument("--%s" % harness, required=False,
                            action="store", dest=harness,
                            help="Name of the %s zip." % harness)
    parser.add_argument("--dest-file", required=True,
                        action="store", dest="destfile",
                        help="Path to the output file to be written.")
    return parser.parse_args()

def generate_package_data(args):
    # Generate a dictionary mapping test harness names (exactly as they're known to
    # mozharness and testsuite-targets.mk, ideally) to the set of archive names that
    # harness depends on to run.
    # mozharness will use this file to determine what test zips to download,
    # which will be an optimization once parts of the main zip are split to harness
    # specific zips.
    tests_common = args.tests_common
    if args.use_short_names:
        tests_common = 'target.common.tests.zip'

    jsshell = args.jsshell

    harness_requirements = dict([(k, [tests_common]) for k in ALL_HARNESSES])
    harness_requirements['jittest'].append(jsshell)
    for harness in PACKAGE_SPECIFIED_HARNESSES + OPTIONAL_PACKAGES:
        pkg_name = getattr(args, harness, None)
        if pkg_name is None:
            continue
        if args.use_short_names:
            pkg_name = 'target.%s.tests.zip' % harness
        harness_requirements[harness].append(pkg_name)
    return harness_requirements

if __name__ == '__main__':
    args = parse_args()
    packages_data = generate_package_data(args)
    with open(args.destfile, 'w') as of:
        json.dump(packages_data, of, indent=4)
