# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import atexit
import os
import re
import shutil
import sys

from run_operations import run_git, run_hg, run_shell

# This script saves the mozilla patch stack and no-op commit tracking
# files.  This makes our fast-forward process much more resilient by
# saving the intermediate state after each upstream commit is processed.

error_help = None
script_name = os.path.basename(__file__)


@atexit.register
def early_exit_handler():
    print("*** ERROR *** {} did not complete successfully".format(script_name))
    if error_help is not None:
        print(error_help)


def save_patch_stack(
    github_path,
    github_branch,
    patch_directory,
    state_directory,
    target_branch_head,
    bug_number,
):
    # remove the current patch files
    files_to_remove = os.listdir(patch_directory)
    for file in files_to_remove:
        os.remove(os.path.join(patch_directory, file))

    # find the base of the patch stack
    cmd = "git merge-base {} {}".format(github_branch, target_branch_head)
    stdout_lines = run_git(cmd, github_path)
    merge_base = stdout_lines[0]

    # grab patch stack
    cmd = "git format-patch --keep-subject --no-signature --output-directory {} {}..{}".format(
        patch_directory, merge_base, github_branch
    )
    run_git(cmd, github_path)

    # remove the commit summary from the file name
    patches_to_rename = os.listdir(patch_directory)
    for file in patches_to_rename:
        shortened_name = re.sub("^(\d\d\d\d)-.*\.patch", "\\1.patch", file)
        os.rename(
            os.path.join(patch_directory, file),
            os.path.join(patch_directory, shortened_name),
        )

    # remove the unhelpful first line of the patch files that only
    # causes diff churn.  For reasons why we can't skip creating backup
    # files during the in-place editing, see:
    # https://stackoverflow.com/questions/5694228/sed-in-place-flag-that-works-both-on-mac-bsd-and-linux
    run_shell("sed -i'.bak' -e '1d' {}/*.patch".format(patch_directory))
    run_shell("rm {}/*.patch.bak".format(patch_directory))

    # it is also helpful to save the no-op-cherry-pick-msg files from
    # the state directory so that if we're restoring a patch-stack we
    # also restore the possibly consumed no-op tracking files.
    no_op_files = [
        path
        for path in os.listdir(state_directory)
        if re.findall(".*no-op-cherry-pick-msg$", path)
    ]
    for file in no_op_files:
        shutil.copy(os.path.join(state_directory, file), patch_directory)

    # get missing files (that should be marked removed)
    cmd = "hg status --no-status --deleted {}".format(patch_directory)
    stdout_lines = run_hg(cmd)
    if len(stdout_lines) != 0:
        cmd = "hg rm {}".format(" ".join(stdout_lines))
        run_hg(cmd)

    # get unknown files (that should be marked added)
    cmd = "hg status --no-status --unknown {}".format(patch_directory)
    stdout_lines = run_hg(cmd)
    if len(stdout_lines) != 0:
        cmd = "hg add {}".format(" ".join(stdout_lines))
        run_hg(cmd)

    # if any files are marked for add/remove/modify, commit them
    cmd = "hg status --added --removed --modified {}".format(patch_directory)
    stdout_lines = run_hg(cmd)
    if (len(stdout_lines)) != 0:
        print("Updating {} files in {}".format(len(stdout_lines), patch_directory))
        if bug_number is None:
            run_hg("hg amend")
        else:
            run_shell(
                "hg commit --message 'Bug {} - updated libwebrtc patch stack'".format(
                    bug_number
                )
            )


if __name__ == "__main__":
    default_patch_dir = "third_party/libwebrtc/moz-patch-stack"
    default_script_dir = "dom/media/webrtc/third_party_build"
    default_state_dir = ".moz-fast-forward"

    parser = argparse.ArgumentParser(
        description="Save moz-libwebrtc github patch stack"
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
        "--state-path",
        default=default_state_dir,
        help="path to state directory (defaults to {})".format(default_state_dir),
    )
    parser.add_argument(
        "--target-branch-head",
        required=True,
        help="target branch head for fast-forward, should match MOZ_TARGET_UPSTREAM_BRANCH_HEAD in config_env",
    )
    parser.add_argument(
        "--script-path",
        default=default_script_dir,
        help="path to script directory (defaults to {})".format(default_script_dir),
    )
    parser.add_argument(
        "--separate-commit-bug-number",
        type=int,
        help="integer Bugzilla number (example: 1800920), if provided will write patch stack as separate commit",
    )
    parser.add_argument(
        "--skip-startup-sanity",
        action="store_true",
        default=False,
        help="skip checking for clean repo and doing the initial verify vendoring",
    )
    args = parser.parse_args()

    if not args.skip_startup_sanity:
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

        print("Verifying vendoring before saving patch-stack...")
        run_shell("bash {}/verify_vendoring.sh".format(args.script_path), False)

    save_patch_stack(
        args.repo_path,
        args.branch,
        os.path.abspath(args.patch_path),
        args.state_path,
        args.target_branch_head,
        args.separate_commit_bug_number,
    )

    # unregister the exit handler so the normal exit doesn't falsely
    # report as an error.
    atexit.unregister(early_exit_handler)
