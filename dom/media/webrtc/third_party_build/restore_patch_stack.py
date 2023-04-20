# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import re
import shutil

from fetch_github_repo import fetch_repo
from run_operations import run_git, run_shell

# This script restores the mozilla patch stack and no-op commit tracking
# files.  In the case of repo corruption or a mistake made during
# various rebase conflict resolution operations, the patch-stack can be
# restored rather than requiring the user to restart the fast-forward
# process from the beginning.


def get_last_line(file_path):
    # technique from https://stackoverflow.com/questions/46258499/how-to-read-the-last-line-of-a-file-in-python
    with open(file_path, "rb") as f:
        try:  # catch OSError in case of a one line file
            f.seek(-2, os.SEEK_END)
            while f.read(1) != b"\n":
                f.seek(-2, os.SEEK_CUR)
        except OSError:
            f.seek(0)
        return f.readline().decode().strip()


def restore_patch_stack(
    github_path, github_branch, patch_directory, state_directory, tar_name
):
    # first, refetch the repo (hopefully utilizing the tarfile for speed) so
    # the patches apply cleanly
    fetch_repo(github_path, True, os.path.join(state_directory, tar_name))

    # remove any stale no-op-cherry-pick-msg files in state_directory
    run_shell("rm {}/*.no-op-cherry-pick-msg || true".format(state_directory))

    # lookup latest vendored commit from third_party/libwebrtc/README.moz-ff-commit
    file = os.path.abspath("third_party/libwebrtc/README.moz-ff-commit")
    last_vendored_commit = get_last_line(file)

    # checkout the previous vendored commit with proper branch name
    cmd = "git checkout -b {} {}".format(github_branch, last_vendored_commit)
    run_git(cmd, github_path)

    # restore the patches to moz-libwebrtc repo, use run_shell instead of
    # run_hg to allow filepath wildcard
    print("Restoring patch stack")
    run_shell("cd {} && git am {}/*.patch".format(github_path, patch_directory))

    # it is also helpful to restore the no-op-cherry-pick-msg files to
    # the state directory so that if we're restoring a patch-stack we
    # also restore the possibly consumed no-op tracking files.
    no_op_files = [
        path
        for path in os.listdir(patch_directory)
        if re.findall(".*no-op-cherry-pick-msg$", path)
    ]
    for file in no_op_files:
        shutil.copy(os.path.join(patch_directory, file), state_directory)


if __name__ == "__main__":
    default_patch_dir = "third_party/libwebrtc/moz-patch-stack"
    default_state_dir = ".moz-fast-forward"
    default_tar_name = "moz-libwebrtc.tar.gz"

    parser = argparse.ArgumentParser(
        description="Restore moz-libwebrtc github patch stack"
    )
    parser.add_argument(
        "--repo-path",
        required=True,
        help="path to libwebrtc repo",
    )
    parser.add_argument(
        "--branch",
        default="mozpatches",
        help="moz-libwebrtc branch (defaults to mozpatches)",
    )
    parser.add_argument(
        "--patch-path",
        default=default_patch_dir,
        help="path to save patches (defaults to {})".format(default_patch_dir),
    )
    parser.add_argument(
        "--tar-name",
        default=default_tar_name,
        help="name of tar file (defaults to {})".format(default_tar_name),
    )
    parser.add_argument(
        "--state-path",
        default=default_state_dir,
        help="path to state directory (defaults to {})".format(default_state_dir),
    )
    args = parser.parse_args()

    restore_patch_stack(
        args.repo_path,
        args.branch,
        os.path.abspath(args.patch_path),
        args.state_path,
        args.tar_name,
    )
