# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import importlib
import re
import subprocess
import sys

sys.path.insert(0, "./dom/media/webrtc/third_party_build")
vendor_libwebrtc = importlib.import_module("vendor-libwebrtc")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Get relevant change count from an upstream git commit"
    )
    parser.add_argument(
        "--repo-path",
        required=True,
        help="path to libwebrtc repo",
    )
    parser.add_argument("--commit-sha", required=True, help="sha of commit to examine")
    parser.add_argument("--diff-filter", choices=("A", "D", "R"))
    args = parser.parse_args()

    command = [
        "git",
        "show",
        "--oneline",
        "--name-status",
        "--pretty=format:",
        None if not args.diff_filter else "--diff-filter={}".format(args.diff_filter),
        args.commit_sha,
    ]
    # strip possible empty elements from command list
    command = [x for x in command if x is not None]

    # Get the list of changes in the upstream commit.
    res = subprocess.run(
        command,
        capture_output=True,
        text=True,
        cwd=args.repo_path,
    )
    if res.returncode != 0:
        sys.exit("error: {}".format(res.stderr.strip()))

    changed_files = [line.strip() for line in res.stdout.strip().split("\n")]
    changed_files = [line for line in changed_files if line != ""]

    # Fetch the list of excludes and includes used in the vendoring script.
    exclude_list = vendor_libwebrtc.get_excluded_paths()
    include_list = vendor_libwebrtc.get_included_path_overrides()

    # First, search for changes in files that are specifically included.
    # Do this first, because some of these files might be filtered out
    # by the exclude list.
    regex_includes = "|".join(["^.\t{}".format(i) for i in include_list])
    included_files = [
        path for path in changed_files if re.findall(regex_includes, path)
    ]

    # Convert the exclude list to a regex string.
    regex_excludes = "|".join(["^.\t{}".format(i) for i in exclude_list])

    # Filter out the excluded files/paths.
    files_not_excluded = [
        path for path in changed_files if not re.findall(regex_excludes, path)
    ]

    for path in included_files + files_not_excluded:
        print(path)
