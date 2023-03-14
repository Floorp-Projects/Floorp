#!/bin/bash

# This script is a simple helper script that creates a branch name
# in moz-libwebrtc and pushes it to origin.  You will need permissions
# to the mozilla fork of libwebrtc in order to complete this operation.
# The repo is: https://github.com/mozilla/libwebrtc/
#
# Note: this should only be run after elm has been merged to mozilla-central

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh

# After this point:
# * eE: All commands should succeed.
# * o pipefail: All stages of all pipes should succeed.
set -eEo pipefail

cd $MOZ_LIBWEBRTC_SRC

git fetch

if git show-ref --quiet refs/remotes/origin/$MOZ_LIBWEBRTC_OFFICIAL_BRANCH; then
  echo "Branch '$MOZ_LIBWEBRTC_OFFICIAL_BRANCH' already exists remotely. Nothing to do."

  echo "Please ensure this branch information can be found on Bug $MOZ_FASTFORWARD_BUG"
  echo "https://github.com/mozilla/libwebrtc/tree/$MOZ_LIBWEBRTC_OFFICIAL_BRANCH"
  exit 0
fi

if git show-ref --quiet refs/heads/$MOZ_LIBWEBRTC_OFFICIAL_BRANCH; then
  echo "Branch '$MOZ_LIBWEBRTC_OFFICIAL_BRANCH' already exists locally. No need to create it."
else
  echo "Creating branch '$MOZ_LIBWEBRTC_OFFICIAL_BRANCH'"
  git branch $MOZ_LIBWEBRTC_OFFICIAL_BRANCH
fi

echo "Pushing the branch to https://github.com/mozilla/libwebrtc"
git push origin $MOZ_LIBWEBRTC_OFFICIAL_BRANCH

echo "Please add this new branch information to Bug $MOZ_FASTFORWARD_BUG"
echo "https://github.com/mozilla/libwebrtc/tree/$MOZ_LIBWEBRTC_OFFICIAL_BRANCH"
