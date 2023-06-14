#!/bin/bash

# This script takes the current base sha, the next base sha, and the sha
# of the commit that reverts the next base as determined by
# detect_upstream_revert.sh and "inserts" two commits at the bottom of the
# moz_libwebrtc GitHub patch stack.  The two commits are exact copies of
# the upcoming commit and its corresponding revert commit, with commit
# messages indicating they are temporary commits.  Additionally, 2 files
# are written that act as markers for the fast-forward script and contain
# supplemental commit message text.
#
# When the fast-forward script runs, it will rebase onto the next base
# sha.  Since we have a corresponding, identical temp commit at the bottom
# of our patch stack, the temp commit will be absorbed as unnecessary.
# Since the patch stack now has the temp revert commit at the bottom, this
# results in a “no-op” commit.  The marker file indicates that specially
# handling should occur in the fast-forward-libwebrtc.sh and loop-ff.sh
# scripts.  This special handling includes adding the supplemental commit
# text that explains why the commit is a no-op (or empty) commit and
# skipping the verification of the number of files changed between our
# mercurial repository and the GitHub repository.

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh

# If DEBUG_GEN is set all commands should be printed as they are executed
if [ ! "x$DEBUG_GEN" = "x" ]; then
  set -x
fi

if [ "x$MOZ_LIBWEBRTC_SRC" = "x" ]; then
  echo "MOZ_LIBWEBRTC_SRC is not defined, see README.md"
  exit
fi

if [ -d $MOZ_LIBWEBRTC_SRC ]; then
  echo "MOZ_LIBWEBRTC_SRC is $MOZ_LIBWEBRTC_SRC"
else
  echo "Path $MOZ_LIBWEBRTC_SRC is not found, see README.md"
  exit
fi

if [ "x$MOZ_LIBWEBRTC_BRANCH" = "x" ]; then
  echo "MOZ_LIBWEBRTC_BRANCH is not defined, see README.md"
  exit
fi

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

find_base_commit
find_next_commit
echo "MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
echo "MOZ_LIBWEBRTC_NEXT_BASE: $MOZ_LIBWEBRTC_NEXT_BASE"
echo "MOZ_LIBWEBRTC_REVERT_SHA: $MOZ_LIBWEBRTC_REVERT_SHA"

# These files serve dual purposes:
# 1) They serve as marker/indicator files to loop-ff.sh to
#    know to process the commit differently, accounting for
#    the no-op nature of the commit and it's corresponding
#    revert commit.
# 2) The contain supplemental commit message text to explain
#    why the commits are essentially empty.
# They are written first on the off chance that the rebase
# operation below fails and requires manual intervention,
# thus avoiding the operator of these scripts to remember to
# generate these two files.
echo $"Essentially a no-op since we're going to see this change
reverted when we vendor in $MOZ_LIBWEBRTC_REVERT_SHA." \
> $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg

echo "We already cherry-picked this when we vendored $MOZ_LIBWEBRTC_NEXT_BASE." \
> $STATE_DIR/$MOZ_LIBWEBRTC_REVERT_SHA.no-op-cherry-pick-msg

cd $MOZ_LIBWEBRTC_SRC
git checkout -b moz-cherry-pick $MOZ_LIBWEBRTC_BASE

COMMIT_MSG_FILE=$TMP_DIR/commit.msg

# build commit message with annotated summary
git show --format='%s%n%n%b' --no-patch $MOZ_LIBWEBRTC_NEXT_BASE > $COMMIT_MSG_FILE
ed -s $COMMIT_MSG_FILE <<EOF
1,s/^\(.*\)$/(tmp-cherry-pick) & ($MOZ_LIBWEBRTC_NEXT_BASE)/
w
q
EOF
git cherry-pick --no-commit $MOZ_LIBWEBRTC_NEXT_BASE
git commit --file $COMMIT_MSG_FILE

# build commit message with annotated summary
git show --format='%s%n%n%b' --no-patch $MOZ_LIBWEBRTC_REVERT_SHA > $COMMIT_MSG_FILE
ed -s $COMMIT_MSG_FILE <<EOF
1,s/^\(.*\)$/(tmp-cherry-pick) & ($MOZ_LIBWEBRTC_REVERT_SHA)/
w
q
EOF
git cherry-pick --no-commit $MOZ_LIBWEBRTC_REVERT_SHA
git commit --file $COMMIT_MSG_FILE

git checkout $MOZ_LIBWEBRTC_BRANCH
git rebase moz-cherry-pick
git branch -d moz-cherry-pick

