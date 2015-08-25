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
    parser.add_argument("--upload-properties", required=False,
                        action="store", dest="upload_properties",
                        help="Path to the properties written by 'make upload'")
    args = parser.parse_args()

    json_data = getMarProperties(args.complete_mar_file)
    if args.upload_properties:
        with open(args.upload_properties) as f:
            json_data.update(json.load(f))
    if args.partial_mar_file:
        json_data.update(getMarProperties(args.partial_mar_file, partial=True))

        # Pull the previous buildid from the partial mar filename.
        res = re.match(r'.*\.([0-9]+)-[0-9]+.mar', args.partial_mar_file)
        if res:
            json_data['previous_buildid'] = res.group(1)

            # Set partialInfo to be a collection of the partial mar properties
            # useful for balrog.
            json_data['partialInfo'] = getPartialInfo(json_data)

    with open('mach_build_properties.json', 'w') as outfile:
        json.dump(json_data, outfile, indent=4)
