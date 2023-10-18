# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import atexit
import os
import shutil
import sys

from filter_git_changes import filter_git_changes
from run_operations import (
    get_last_line,
    run_git,
    run_hg,
    run_shell,
    update_resume_state,
)
from vendor_and_commit import vendor_and_commit

# This script cherry-picks an upstream commit with the appropriate
# commit message, and adds the no-op commit tracking file for the when
# we vendor the upstream commit later.

error_help = None
script_name = os.path.basename(__file__)


@atexit.register
def early_exit_handler():
    print("*** ERROR *** {} did not complete successfully".format(script_name))
    if error_help is not None:
        print(error_help)


def write_commit_message_file(
    commit_message_filename,
    github_path,
    github_sha,
    bug_number,
    reviewers,
):
    print("commit_message_filename: {}".format(commit_message_filename))
    print("github_path: {}".format(github_path))
    print("github_sha: {}".format(github_sha))
    print("bug_number: {}".format(bug_number))

    cmd = "git show --format=%H --no-patch {}".format(github_sha)
    stdout_lines = run_git(cmd, github_path)
    github_long_sha = stdout_lines[0]
    print("github_long_sha: {}".format(github_long_sha))

    cmd = "git show --format=%s%n%n%b --no-patch {}".format(github_sha)
    github_commit_msg_lines = run_git(cmd, github_path)

    with open(commit_message_filename, "w") as ofile:
        ofile.write(
            "Bug {} - Cherry-pick upstream libwebrtc commit {} r?{}".format(
                bug_number, github_sha, reviewers
            )
        )
        ofile.write("\n")
        ofile.write("\n")
        ofile.write(
            "Upstream commit: https://webrtc.googlesource.com/src/+/{}".format(
                github_long_sha
            )
        )
        ofile.write("\n")
        for line in github_commit_msg_lines:
            ofile.write("       {}".format(line))
            ofile.write("\n")


def cherry_pick_commit(
    commit_message_filename,
    github_path,
    github_sha,
):
    print("commit_message_filename: {}".format(commit_message_filename))
    print("github_path: {}".format(github_path))
    print("github_sha: {}".format(github_sha))

    cmd = "git cherry-pick --no-commit {}".format(github_sha)
    run_git(cmd, github_path)

    cmd = "git commit --file {}".format(os.path.abspath(commit_message_filename))
    run_git(cmd, github_path)


def write_noop_tracking_file(
    github_sha,
    bug_number,
):
    noop_basename = "{}.no-op-cherry-pick-msg".format(github_sha)
    noop_filename = os.path.join(args.state_path, noop_basename)
    print("noop_filename: {}".format(noop_filename))
    with open(noop_filename, "w") as ofile:
        ofile.write("We cherry-picked this in bug {}".format(bug_number))
        ofile.write("\n")
    shutil.copy(noop_filename, args.patch_path)
    cmd = "hg add {}".format(os.path.join(args.patch_path, noop_basename))
    run_hg(cmd)
    cmd = "hg amend {}".format(os.path.join(args.patch_path, noop_basename))
    run_hg(cmd)


if __name__ == "__main__":
    default_target_dir = "third_party/libwebrtc"
    default_state_dir = ".moz-fast-forward"
    default_log_dir = ".moz-fast-forward/logs"
    default_tmp_dir = ".moz-fast-forward/tmp"
    default_script_dir = "dom/media/webrtc/third_party_build"
    default_patch_dir = "third_party/libwebrtc/moz-patch-stack"
    default_repo_dir = ".moz-fast-forward/moz-libwebrtc"

    parser = argparse.ArgumentParser(
        description="Cherry-pick upstream libwebrtc commit"
    )
    parser.add_argument(
        "--target-path",
        default=default_target_dir,
        help="target path for vendoring (defaults to {})".format(default_target_dir),
    )
    parser.add_argument(
        "--state-path",
        default=default_state_dir,
        help="path to state directory (defaults to {})".format(default_state_dir),
    )
    parser.add_argument(
        "--log-path",
        default=default_log_dir,
        help="path to log directory (defaults to {})".format(default_log_dir),
    )
    parser.add_argument(
        "--tmp-path",
        default=default_tmp_dir,
        help="path to tmp directory (defaults to {})".format(default_tmp_dir),
    )
    parser.add_argument(
        "--script-path",
        default=default_script_dir,
        help="path to script directory (defaults to {})".format(default_script_dir),
    )
    parser.add_argument(
        "--repo-path",
        default=default_repo_dir,
        help="path to moz-libwebrtc repo (defaults to {})".format(default_repo_dir),
    )
    parser.add_argument(
        "--commit-sha",
        required=True,
        help="sha of commit to examine",
    )
    parser.add_argument(
        "--branch",
        default="mozpatches",
        help="moz-libwebrtc branch (defaults to mozpatches)",
    )
    parser.add_argument(
        "--commit-bug-number",
        type=int,
        required=True,
        help="integer Bugzilla number (example: 1800920)",
    )
    parser.add_argument(
        "--patch-path",
        default=default_patch_dir,
        help="path to save patches (defaults to {})".format(default_patch_dir),
    )
    parser.add_argument(
        "--reviewers",
        required=True,
        help='reviewers for cherry-picked patch (like "ng,mjf")',
    )
    args = parser.parse_args()

    # make sure the mercurial repo is clean before beginning
    error_help = (
        "There are modified or untracked files in the mercurial repo.\n"
        "Please start with a clean repo before running {}"
    ).format(script_name)
    stdout_lines = run_hg("hg status")
    if len(stdout_lines) != 0:
        sys.exit(1)

    # make sure the github repo exists
    error_help = (
        "No moz-libwebrtc github repo found at {}\n"
        "Please run restore_patch_stack.py before running {}"
    ).format(args.repo_path, script_name)
    if not os.path.exists(args.repo_path):
        sys.exit(1)
    error_help = None

    commit_message_filename = os.path.join(args.tmp_path, "cherry-pick-commit_msg.txt")

    resume_state_filename = os.path.join(args.state_path, "cherry_pick_commit.resume")
    resume_state = ""
    if os.path.exists(resume_state_filename):
        resume_state = get_last_line(resume_state_filename).strip()
    print("resume_state: '{}'".format(resume_state))

    if len(resume_state) == 0:
        update_resume_state("resume2", resume_state_filename)
        print("-------")
        print("------- write commit message file {}".format(commit_message_filename))
        print("-------")
        write_commit_message_file(
            commit_message_filename,
            args.repo_path,
            args.commit_sha,
            args.commit_bug_number,
            args.reviewers,
        )

    error_help = (
        "The cherry-pick operation of {} has failed.\n"
        "To fix this issue, you will need to jump to the github\n"
        "repo at {} .\n"
        "Please resolve all the cherry-pick conflicts, and commit the changes\n"
        "using:\n"
        "    git commit --file {}\n"
        "When the github cherry-pick is complete, re-run this script to resume\n"
        "the cherry-pick process."
    ).format(args.commit_sha, args.repo_path, os.path.abspath(commit_message_filename))
    if len(resume_state) == 0 or resume_state == "resume2":
        resume_state = ""
        update_resume_state("resume3", resume_state_filename)
        print("-------")
        print("------- cherry-pick {} into {}".format(args.commit_sha, args.repo_path))
        print("-------")
        cherry_pick_commit(
            commit_message_filename,
            args.repo_path,
            args.commit_sha,
        )
    error_help = None

    if len(resume_state) == 0 or resume_state == "resume3":
        resume_state = ""
        update_resume_state("resume4", resume_state_filename)
        print("-------")
        print("------- vendor from {}".format(args.repo_path))
        print("-------")
        vendor_and_commit(
            args.script_path,
            args.repo_path,
            args.branch,
            args.commit_sha,
            args.target_path,  # os.path.abspath(args.target_path),
            args.state_path,
            args.log_path,
            os.path.join(args.tmp_path, "cherry-pick-commit_msg.txt"),
        )

        # The normal vendoring process updates README.mozilla with info
        # on what commit was vendored and the command line used to do
        # the vendoring.  Since we're only reusing the vendoring script
        # here, we don't want to update the README.mozilla file.
        cmd = "hg revert -r tip^ third_party/libwebrtc/README.mozilla"
        run_hg(cmd)
        cmd = "hg amend"
        run_hg(cmd)

        # get the files changed from the newly vendored cherry-pick
        # commit in mercurial
        cmd = "hg status --change tip --exclude '**/README.*'"
        stdout_lines = run_shell(cmd)  # run_shell to allow file wildcard
        print("Mercurial changes:\n{}".format(stdout_lines))
        hg_file_change_cnt = len(stdout_lines)

        # get the files changed from the original cherry-picked patch in
        # our github repo (moz-libwebrtc)
        git_paths_changed = filter_git_changes(args.repo_path, args.commit_sha, None)
        print("github changes:\n{}".format(git_paths_changed))
        git_file_change_cnt = len(git_paths_changed)

        error_help = (
            "Vendoring the cherry-pick of commit {} has failed due to mismatched\n"
            "changed file counts between mercurial ({}) and git ({}).\n"
            "This may be because the mozilla patch-stack was not verified after\n"
            "running restore_patch_stack.py.  After reconciling the changes in\n"
            "the newly committed mercurial patch, please re-run {} to complete\n"
            "the cherry-pick processing."
        ).format(args.commit_sha, hg_file_change_cnt, git_file_change_cnt, script_name)
        if hg_file_change_cnt != git_file_change_cnt:
            sys.exit(1)
        error_help = None

    if len(resume_state) == 0 or resume_state == "resume4":
        resume_state = ""
        update_resume_state("", resume_state_filename)
        print("-------")
        print("------- write the noop tracking file")
        print("-------")
        write_noop_tracking_file(args.commit_sha, args.commit_bug_number)

    # unregister the exit handler so the normal exit doesn't falsely
    # report as an error.
    atexit.unregister(early_exit_handler)
