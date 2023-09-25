#!/bin/bash

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh
export HGPLAIN=1

echo "MOZ_LIBWEBRTC_SRC: $MOZ_LIBWEBRTC_SRC"
echo "MOZ_LIBWEBRTC_BRANCH: $MOZ_LIBWEBRTC_BRANCH"
echo "MOZ_FASTFORWARD_BUG: $MOZ_FASTFORWARD_BUG"

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

# wipe resume_state for new run
rm -f $STATE_DIR/resume_state

# If there is no cache file for the branch-head lookups done in
# update_default_config.sh, go ahead and copy our small pre-warmed
# version.
if [ ! -f $STATE_DIR/milestone.cache ]; then
  cp $SCRIPT_DIR/pre-warmed-milestone.cache $STATE_DIR/milestone.cache
fi

# If there is no .mozconfig file, copy a basic one from default_mozconfig
if [ ! -f .mozconfig ]; then
  cp $SCRIPT_DIR/default_mozconfig .mozconfig
fi

# fetch the github repro
./mach python $SCRIPT_DIR/fetch_github_repo.py \
    --repo-path $MOZ_LIBWEBRTC_SRC \
    --state-path $STATE_DIR

CURRENT_DIR=`pwd`
cd $MOZ_LIBWEBRTC_SRC

# clear any possible previous patches
rm -f *.patch

# create a new work branch and "export" a new patch stack to rebase
# find the common commit between our upstream release branch and trunk
CHERRY_PICK_BASE=`git merge-base branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM master`
echo "common commit: $CHERRY_PICK_BASE"

# create a new branch at the common commit and checkout the new branch
ERROR_HELP=$"
Unable to create branch '$MOZ_LIBWEBRTC_BRANCH'.  This probably means
that prep_repo.sh is being called on a repo that already has a patch
stack in progress.  If you're sure you want to do this, the following
commands will allow the process to continue:
  ( cd $MOZ_LIBWEBRTC_SRC && \\
    git checkout $MOZ_LIBWEBRTC_BRANCH && \\
    git checkout -b $MOZ_LIBWEBRTC_BRANCH-old && \\
    git branch -D $MOZ_LIBWEBRTC_BRANCH ) && \\
  bash $0
"
git branch $MOZ_LIBWEBRTC_BRANCH $CHERRY_PICK_BASE
ERROR_HELP=""
git checkout $MOZ_LIBWEBRTC_BRANCH

# make sure we're starting with a clean tmp directory
rm -f $TMP_DIR/*.patch $TMP_DIR/*.patch.bak

# grab the patches for all the commits in chrome's release branch for libwebrtc
git format-patch -o $TMP_DIR -k $CHERRY_PICK_BASE..branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM
# tweak the release branch commit summaries to show they were cherry picked
sed -i.bak -e "/^Subject: / s/^Subject: /Subject: (cherry-pick-branch-heads\/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM) /" $TMP_DIR/*.patch
git am $TMP_DIR/*.patch # applies to branch mozpatches
rm $TMP_DIR/*.patch $TMP_DIR/*.patch.bak

# we don't use restore_patch_stack.py here because it would overwrite the patches
# from the previous release branch we just added in the above step.

# grab all the moz patches and apply
git am $CURRENT_DIR/third_party/libwebrtc/moz-patch-stack/*.patch

cd $CURRENT_DIR

# cp all the no-op files to STATE_DIR
NO_OP_FILE_COUNT=`ls third_party/libwebrtc/moz-patch-stack \
    | grep "no-op-cherry-pick-msg" | wc -l | tr -d " " || true`
if [ "x$NO_OP_FILE_COUNT" != "x0" ]; then
  cp $CURRENT_DIR/third_party/libwebrtc/moz-patch-stack/*.no-op-cherry-pick-msg \
     $STATE_DIR
fi

bash $SCRIPT_DIR/verify_vendoring.sh || exit 1
