#!/bin/bash

SCRIPT_DIR="dom/media/webrtc/third_party_build"

# Allow user to override default path to config_env
if [ "x$MOZ_CONFIG_PATH" = "x" ]; then
  MOZ_CONFIG_PATH=~/config_env
  echo "Using default MOZ_CONFIG_PATH=$MOZ_CONFIG_PATH"
fi

if [ ! -f $MOZ_CONFIG_PATH ]; then
  echo "Missing $MOZ_CONFIG_PATH"
  echo "Please copy $SCRIPT_DIR/example_config_env"
  echo "and edit (at least) MOZ_LIBWEBRTC_SRC to match your environment."
  echo ""
  echo "cp $SCRIPT_DIR/example_config_env $MOZ_CONFIG_PATH"
  echo ""
  exit 1
fi
source $MOZ_CONFIG_PATH

function find_base_commit()
{
  # read the last line of README.moz-ff-commit to retrieve our current base
  # commit in moz-libwebrtc
  MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`
  echo "prelim MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
  # if we've advanced into a chrome release branch, we need to adjust the
  # MOZ_LIBWEBRTC_BASE to the last common commit so we can now advance up
  # the trunk commits.
  MOZ_LIBWEBRTC_BASE=`cd $MOZ_LIBWEBRTC_SRC ; git merge-base $MOZ_LIBWEBRTC_BASE $MOZ_GIT_RELEASE_BRANCH`
  # now make it a short hash
  MOZ_LIBWEBRTC_BASE=`cd $MOZ_LIBWEBRTC_SRC ; git rev-parse --short $MOZ_LIBWEBRTC_BASE`
  echo "adjusted MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
}

function find_next_commit()
{
  # identify the next commit above our current base commit
  MOZ_LIBWEBRTC_NEXT_BASE=`cd $MOZ_LIBWEBRTC_SRC ; \
  git log --oneline --ancestry-path $MOZ_LIBWEBRTC_BASE^..$MOZ_GIT_RELEASE_BRANCH \
   | tail -2 | head -1 | awk '{print $1;}'`
}
