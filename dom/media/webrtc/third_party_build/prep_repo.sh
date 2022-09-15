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
git checkout master
git remote add upstream https://webrtc.googlesource.com/src
git fetch upstream
git merge upstream/master

# fetch upstream's branch-heads/xxxx
git config --local --add remote.upstream.fetch '+refs/branch-heads/*:refs/remotes/branch-heads/*'
git fetch upstream

# checkout our previous branch to make sure commits are visible in the repo
git checkout $MOZ_PRIOR_GIT_BRANCH

# create a new work branch and "export" a new patch stack to rebase
git branch $MOZ_LIBWEBRTC_COMMIT $MOZ_LIBWEBRTC_BASE
git checkout $MOZ_LIBWEBRTC_COMMIT
git format-patch -k $MOZ_LIBWEBRTC_BASE..$MOZ_PRIOR_GIT_BRANCH
git am *.patch # applies to branch mozpatches
rm *.patch

cd $CURRENT_DIR

bash dom/media/webrtc/third_party_build/verify_vendoring.sh || true
