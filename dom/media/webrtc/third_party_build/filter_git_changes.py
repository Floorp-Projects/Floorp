# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import importlib
import re
import sys

sys.path.insert(0, "./dom/media/webrtc/third_party_build")
vendor_libwebrtc = importlib.import_module("vendor-libwebrtc")

from run_operations import run_git


def filter_git_changes(github_path, commit_sha, diff_filter):
    command = [
        "git",
        "show",
        "--oneline",
        "--name-status",
        "--pretty=format:",
        None if not diff_filter else "--diff-filter={}".format(diff_filter),
        commit_sha,
    ]
    # strip possible empty elements from command list
    command = " ".join([x for x in command if x is not None])

    # Get the list of changes in the upstream commit.
    stdout_lines = run_git(command, github_path)

    changed_files = [line.strip() for line in stdout_lines]
    changed_files = [line for line in changed_files if line != ""]

    # Fetch the list of excludes and includes used in the vendoring script.
    exclude_file_list = vendor_libwebrtc.get_excluded_files()
    exclude_dir_list = vendor_libwebrtc.get_excluded_dirs()
    include_list = vendor_libwebrtc.get_included_path_overrides()

    # First, search for changes in files that are specifically included.
    # Do this first, because some of these files might be filtered out
    # by the exclude list.
    regex_includes = "|".join(["^.\t{}$".format(i) for i in include_list])
    included_files = [
        path for path in changed_files if re.findall(regex_includes, path)
    ]

    # Convert the directory exclude list to a regex string and filter
    # out the excluded directory paths (note the lack of trailing '$'
    # in the regex).
    regex_excludes = "|".join(
        ["^(M|A|D|R\d\d\d)\t{}".format(i) for i in exclude_dir_list]
    )
    files_not_excluded = [
        path for path in changed_files if not re.findall(regex_excludes, path)
    ]

    # Convert the file exclude list to a regex string and filter out the
    # excluded file paths.  The trailing '$' in the regex ensures that
    # we can exclude, for example, '.vpython' and not '.vpython3'.
    regex_excludes = "|".join(["^.\t{}$".format(i) for i in exclude_file_list])
    files_not_excluded = [
        path for path in files_not_excluded if not re.findall(regex_excludes, path)
    ]

    return included_files + files_not_excluded


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
    parser.add_argument(
        "--diff-filter",
        choices=("A", "D", "R"),
        help="filter for adds (A), deletes (D), or renames (R)",
    )
    args = parser.parse_args()

    paths = filter_git_changes(args.repo_path, args.commit_sha, args.diff_filter)
    for path in paths:
        print(path)
