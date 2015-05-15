#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import hashlib
import json
import re
import errno
from argparse import ArgumentParser

def getFileHashAndSize(filename):
    sha512Hash = 'UNKNOWN'
    size = 'UNKNOWN'

    try:
        # open in binary mode to make sure we get consistent results
        # across all platforms
        f = open(filename, "rb")
        shaObj = hashlib.sha512(f.read())
        sha512Hash = shaObj.hexdigest()

        size = os.path.getsize(filename)
    except:
        pass

    return (sha512Hash, size)

def getMarProperties(filename, partial=False):
    if not os.path.exists(filename):
        return {}
    (mar_hash, mar_size) = getFileHashAndSize(filename)
    martype = 'partial' if partial else 'complete'
    return {
        '%sMarFilename' % martype: os.path.basename(filename),
        '%sMarSize' % martype: mar_size,
        '%sMarHash' % martype: mar_hash,
    }

def getUrlProperties(filename, package):
    # let's create a switch case using name-spaces/dict
    # rather than a long if/else with duplicate code
    property_conditions = [
        # key: property name, value: condition
        ('symbolsUrl', lambda m: m.endswith('crashreporter-symbols.zip') or
                       m.endswith('crashreporter-symbols-full.zip')),
        ('testsUrl', lambda m: m.endswith(('tests.tar.bz2', 'tests.zip'))),
        ('unsignedApkUrl', lambda m: m.endswith('apk') and
                           'unsigned-unaligned' in m),
        ('robocopApkUrl', lambda m: m.endswith('apk') and 'robocop' in m),
        ('jsshellUrl', lambda m: 'jsshell-' in m and m.endswith('.zip')),
        ('completeMarUrl', lambda m: m.endswith('.complete.mar')),
        ('partialMarUrl', lambda m: m.endswith('.mar') and '.partial.' in m),
        ('codeCoverageURL', lambda m: m.endswith('code-coverage-gcno.zip')),
        ('sdkUrl', lambda m: m.endswith(('sdk.tar.bz2', 'sdk.zip'))),
        ('testPackagesUrl', lambda m: m.endswith('test_packages.json')),
        ('packageUrl', lambda m: m.endswith(package)),
    ]
    url_re = re.compile(r'''^(https?://.*?\.(?:tar\.bz2|dmg|zip|apk|rpm|mar|tar\.gz|json))$''')
    properties = {}

    try:
        with open(filename) as f:
            for line in f:
                m = url_re.match(line)
                if m:
                    m = m.group(1)
                    for prop, condition in property_conditions:
                        if condition(m):
                            properties.update({prop: m})
                            break
    except IOError as e:
        if e.errno != errno.ENOENT:
            raise
        properties = {prop: 'UNKNOWN' for prop, condition in property_conditions}
    return properties

def getPartialInfo(props):
    return [{
        "from_buildid": props.get("previous_buildid"),
        "size": props.get("partialMarSize"),
        "hash": props.get("partialMarHash"),
        "url": props.get("partialMarUrl"),
    }]

if __name__ == '__main__':
    parser = ArgumentParser(description='Generate mach_build_properties.json for automation builds.')
    parser.add_argument("--complete-mar-file", required=True,
                        action="store", dest="complete_mar_file",
                        help="Path to the complete MAR file, relative to the objdir.")
    parser.add_argument("--partial-mar-file", required=False,
                        action="store", dest="partial_mar_file",
                        help="Path to the partial MAR file, relative to the objdir.")
    parser.add_argument("--upload-output", required=True,
                        action="store", dest="upload_output",
                        help="Path to the text output of 'make upload'")
    parser.add_argument("--upload-files", required=True, nargs="+",
                        action="store", dest="upload_files",
                        help="List of files to be uploaded.")
    parser.add_argument("--package", required=True,
                        action="store", dest="package",
                        help="Filename of the build package")
    args = parser.parse_args()

    json_data = getMarProperties(args.complete_mar_file)
    json_data.update(getUrlProperties(args.upload_output, args.package))
    if args.partial_mar_file:
        json_data.update(getMarProperties(args.partial_mar_file, partial=True))

        # Pull the previous buildid from the partial mar filename.
        res = re.match(r'.*\.([0-9]+)-[0-9]+.mar', args.partial_mar_file)
        if res:
            json_data['previous_buildid'] = res.group(1)

            # Set partialInfo to be a collection of the partial mar properties
            # useful for balrog.
            json_data['partialInfo'] = getPartialInfo(json_data)

    json_data['uploadFiles'] = args.upload_files
    json_data['packageFilename'] = args.package

    with open('mach_build_properties.json', 'w') as outfile:
        json.dump(json_data, outfile, indent=4)
