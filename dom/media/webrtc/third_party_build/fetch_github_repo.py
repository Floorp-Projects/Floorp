# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import re
import shutil
import sys

from run_operations import run_git, run_shell

# This script fetches the moz-libwebrtc github repro with the expected
# upstream remote and branch-heads setup.  This is used by both the
# prep_repo.sh script as well as the restore_patch_stack.py script.
#
# For speed and conservation of network resources, after fetching all
# the data, a tar of the repo is made and used if available.


def fetch_repo(github_path, clone_protocol, force_fetch, tar_path):
    capture_output = False

    # check for pre-existing repo - make sure we force the removal
    if force_fetch and os.path.exists(github_path):
        print("Removing existing repo: {}".format(github_path))
        shutil.rmtree(github_path)

    # To test with ssh (and not give away your default public key):
    # ssh-keygen -t rsa -f ~/.ssh/id_moz_github -q -N ""
    # git -c core.sshCommand="ssh -i ~/.ssh/id_moz_github -o IdentitiesOnly=yes" clone git@github.com:mozilla/libwebrtc.git moz-libwebrtc
    # (cd moz-libwebrtc && git config core.sshCommand "ssh -i ~/.ssh/id_moz_github -o IdentitiesOnly=yes")

    # clone https://github.com/mozilla/libwebrtc
    if not os.path.exists(github_path):
        # check for pre-existing tar, use it if we have it
        if os.path.exists(tar_path):
            print("Using tar file to reconstitute repo")
            cmd = "cd {} ; tar --extract --gunzip --file={}".format(
                os.path.dirname(github_path), os.path.basename(tar_path)
            )
            run_shell(cmd, capture_output)
        else:
            print("Cloning github repo")
            # sure would be nice to have python 3.10's match
            if clone_protocol == "ssh":
                url_prefix = "git@github.com:"
            elif clone_protocol == "https":
                url_prefix = "https://github.com/"
            else:
                print("clone protocol should be either https or ssh")
                sys.exit(1)

            run_shell(
                "git clone {}mozilla/libwebrtc {}".format(url_prefix, github_path),
                capture_output,
            )

    # setup upstream (https://webrtc.googlesource.com/src)
    stdout_lines = run_git("git config --local --list", github_path)
    stdout_lines = [
        path for path in stdout_lines if re.findall("^remote.upstream.url.*", path)
    ]
    if len(stdout_lines) == 0:
        print("Fetching upstream")
        run_git("git checkout master", github_path)
        run_git(
            "git remote add upstream https://webrtc.googlesource.com/src", github_path
        )
        run_git("git fetch upstream", github_path)
        run_git("git merge upstream/master", github_path)
    else:
        print(
            "Upstream remote (https://webrtc.googlesource.com/src) already configured"
        )

    # setup upstream branch-heads
    stdout_lines = run_git(
        "git config --local --get-all remote.upstream.fetch", github_path
    )
    if len(stdout_lines) == 1:
        print("Fetching upstream branch-heads")
        run_git(
            "git config --local --add remote.upstream.fetch +refs/branch-heads/*:refs/remotes/branch-heads/*",
            github_path,
        )
        run_git("git fetch upstream", github_path)
    else:
        print("Upstream remote branch-heads already configured")

    # do a sanity fetch in case this was not a freshly cloned copy of the
    # repo, meaning it may not have all the mozilla branches present.
    run_git("git fetch --all", github_path)

    # create tar to avoid time refetching
    if not os.path.exists(tar_path):
        print("Creating tar file for quicker restore")
        cmd = "cd {} ; tar --create --gzip --file={} {}".format(
            os.path.dirname(github_path),
            os.path.basename(tar_path),
            os.path.basename(github_path),
        )
        run_shell(cmd, capture_output)


if __name__ == "__main__":
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
        "--force-fetch",
        action="store_true",
        default=False,
        help="force rebuild an existing repo directory",
    )
    parser.add_argument(
        "--clone-protocol",
        choices=["https", "ssh"],
        required=True,
        help="Use either https or ssh to clone the git repo",
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

    fetch_repo(
        args.repo_path,
        args.clone_protocol,
        args.force_fetch,
        os.path.join(args.state_path, args.tar_name),
    )
