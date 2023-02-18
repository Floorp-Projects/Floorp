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

echo "MOZ_LIBWEBRTC_SRC: $MOZ_LIBWEBRTC_SRC"
echo "MOZ_LIBWEBRTC_BRANCH: $MOZ_LIBWEBRTC_BRANCH"
echo "MOZ_FASTFORWARD_BUG: $MOZ_FASTFORWARD_BUG"
echo "MOZ_PRIOR_LIBWEBRTC_BRANCH: $MOZ_PRIOR_LIBWEBRTC_BRANCH"

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

# wipe resume_state for new run
rm -f $STATE_DIR/resume_state

# If there is no cache file for the branch-head lookups done in
# update_example_config.sh, go ahead and copy our small pre-warmed
# version.
if [ ! -f $STATE_DIR/milestone.cache ]; then
  cp $SCRIPT_DIR/pre-warmed-milestone.cache $STATE_DIR/milestone.cache
fi

# read the last line of README.moz-ff-commit to retrieve our current base
# commit in moz-libwebrtc
MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`

CURRENT_DIR=`pwd`
cd $MOZ_LIBWEBRTC_SRC

# do a sanity fetch in case this was not a freshly cloned copy of the
# repo, meaning it may not have all the mozilla branches present.
git fetch --all

# pull new upstream commits
if [ "0" == `grep "https://webrtc.googlesource.com/src" .git/config | wc -l | tr -d " " || true` ]; then
  echo "Setting up upstream remote (https://webrtc.googlesource.com/src)."
  git checkout master
  git remote add upstream https://webrtc.googlesource.com/src
  git fetch upstream
  git merge upstream/master
else
  echo "Upstream remote (https://webrtc.googlesource.com/src) already configured."
fi

# fetch upstream's branch-heads/xxxx
if [ "0" == `grep "refs/remotes/branch-heads" .git/config | wc -l | tr -d " " || true` ]; then
  echo "Setting up upstream remote branch-heads."
  git config --local --add remote.upstream.fetch '+refs/branch-heads/*:refs/remotes/branch-heads/*'
  git fetch upstream
else
  echo "Upstream remote branch-heads already configured."
fi

# checkout our previous branch to make sure commits are visible in the repo
git checkout $MOZ_PRIOR_LIBWEBRTC_BRANCH

# clear any possible previous patches
rm -f *.patch

# create a new work branch and "export" a new patch stack to rebase
# find the common commit between our previous work branch and trunk
CHERRY_PICK_BASE=`git merge-base $MOZ_PRIOR_LIBWEBRTC_BRANCH master`
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

# grab the patches for all the commits in chrome's release branch for libwebrtc
git format-patch -o $TMP_DIR -k $CHERRY_PICK_BASE..branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM
# tweak the release branch commit summaries to show they were cherry picked
sed -i.bak -e "/^Subject: / s/^Subject: /Subject: (cherry-pick-branch-heads\/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM) /" $TMP_DIR/*.patch
git am $TMP_DIR/*.patch # applies to branch mozpatches
rm $TMP_DIR/*.patch $TMP_DIR/*.patch.bak

# grab all the moz patches and apply
# git format-patch -k $MOZ_LIBWEBRTC_BASE..$MOZ_PRIOR_LIBWEBRTC_BRANCH
git format-patch -o $TMP_DIR -k branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM..$MOZ_PRIOR_LIBWEBRTC_BRANCH
git am $TMP_DIR/*.patch # applies to branch mozpatches
rm $TMP_DIR/*.patch

cd $CURRENT_DIR

bash $SCRIPT_DIR/verify_vendoring.sh || true
