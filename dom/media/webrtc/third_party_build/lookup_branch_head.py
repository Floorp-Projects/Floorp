# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import json
import os
import pathlib
import sys
import urllib.request

# default cache file location in STATE_DIR location
default_cache_path = ".moz-fast-forward/milestone.cache"


def fetch_branch_head_dict():
    milestone_url = (
        "https://chromiumdash.appspot.com/fetch_milestones?only_branched=true"
    )
    uf = urllib.request.urlopen(milestone_url)
    html = uf.read()
    milestone_dict = json.loads(html)

    # There is more information in the json dictionary, but we only care
    # about the milestone (version) to branch "name" (webrtc_branch)
    # info.  For example:
    #  v106 -> 5249 (which translates to branch-heads/5249)
    #  v107 -> 5304 (which translates to branch-heads/5304)
    #
    # As returned from web query, milestones are integers and branch
    # "names" are strings.
    new_dict = {}
    for row in milestone_dict:
        new_dict[row["milestone"]] = row["webrtc_branch"]

    return new_dict


def read_dict_from_cache(cache_path):
    if cache_path is not None and os.path.exists(cache_path):
        with open(cache_path, "r") as ifile:
            return json.loads(ifile.read(), object_hook=jsonKeys2int)
    return {}


def write_dict_to_cache(cache_path, milestones):
    with open(cache_path, "w") as ofile:
        ofile.write(json.dumps(milestones))


def get_branch_head(milestone, cache_path=default_cache_path):
    milestones = read_dict_from_cache(cache_path)

    # if the cache didn't exist or is stale, try to fetch using a web query
    if milestone not in milestones:
        try:
            milestones = fetch_branch_head_dict()
            write_dict_to_cache(cache_path, milestones)
        except Exception:
            pass

    if milestone in milestones:
        return milestones[milestone]
    return None


# From https://stackoverflow.com/questions/1450957/pythons-json-module-converts-int-dictionary-keys-to-strings
def jsonKeys2int(x):
    if isinstance(x, dict):
        return {int(k): v for k, v in x.items()}
    return x


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Get libwebrtc branch-head for given chromium milestone"
    )
    parser.add_argument(
        "milestone", type=int, help="integer chromium milestone (example: 106)"
    )
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("-c", "--cache", type=pathlib.Path, help="path to cache file")
    args = parser.parse_args()

    # if the user provided a cache path use it, otherwise use the default
    local_cache_path = args.cache or default_cache_path

    branch_head = get_branch_head(args.milestone, local_cache_path)
    if branch_head is None:
        sys.exit("error: chromium milestone '{}' is not found.".format(args.milestone))

    if args.verbose:
        print(
            "chromium milestone {} uses branch-heads/{}".format(
                args.milestone, branch_head
            )
        )
    else:
        print(branch_head)
