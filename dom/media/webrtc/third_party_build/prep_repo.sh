#!/bin/bash

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'echo "*** ERROR *** $? $LINENO $0 did not complete successfully!"' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh

echo "MOZ_LIBWEBRTC_SRC: $MOZ_LIBWEBRTC_SRC"
echo "MOZ_LIBWEBRTC_COMMIT: $MOZ_LIBWEBRTC_COMMIT"
echo "MOZ_FASTFORWARD_BUG: $MOZ_FASTFORWARD_BUG"
echo "MOZ_PRIOR_GIT_BRANCH: $MOZ_PRIOR_GIT_BRANCH"

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

# read the last line of README.moz-ff-commit to retrieve our current base
# commit in moz-libwebrtc
MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`

CURRENT_DIR=`pwd`
cd $MOZ_LIBWEBRTC_SRC

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
git checkout $MOZ_PRIOR_GIT_BRANCH

# clear any possible previous patches
rm -f *.patch

# create a new work branch and "export" a new patch stack to rebase
# find the common commit between our previous work branch and trunk
CHERRY_PICK_BASE=`git merge-base $MOZ_PRIOR_GIT_BRANCH master`
echo "common commit: $CHERRY_PICK_BASE"

# create a new branch at the common commit and checkout the new branch
git branch $MOZ_LIBWEBRTC_COMMIT $CHERRY_PICK_BASE
git checkout $MOZ_LIBWEBRTC_COMMIT

# grab the patches for all the commits in chrome's release branch for libwebrtc
git format-patch -k $CHERRY_PICK_BASE..branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM
# tweak the release branch commit summaries to show they were cherry picked
sed -i.bak -e "/^Subject: / s/^Subject: /Subject: (cherry-pick-branch-heads\/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM) /" *.patch
git am *.patch # applies to branch mozpatches
rm *.patch

# grab all the moz patches and apply
# git format-patch -k $MOZ_LIBWEBRTC_BASE..$MOZ_PRIOR_GIT_BRANCH
git format-patch -k branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM..$MOZ_PRIOR_GIT_BRANCH
git am *.patch # applies to branch mozpatches
rm *.patch

cd $CURRENT_DIR

bash $SCRIPT_DIR/verify_vendoring.sh || true
