#!/bin/bash

CONFIG_PATH=~/config_env
if [ ! -f $CONFIG_PATH ]; then
  echo "Missing $CONFIG_PATH"
  echo "Please copy dom/media/webrtc/third_party_build/example_config_env"
  echo "and edit (at least) MOZ_LIBWEBRTC_SRC to match your environment."
  echo ""
  echo "cp dom/media/webrtc/third_party_build/example_config_env $CONFIG_PATH"
  echo ""
  exit 1
fi
source $CONFIG_PATH

function find_base_commit()
{
  # read the last line of README.moz-ff-commit to retrieve our current base
  # commit in moz-libwebrtc
  MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`
}

function find_next_commit()
{
  # identify the next commit above our current base commit
  MOZ_LIBWEBRTC_NEXT_BASE=`cd $MOZ_LIBWEBRTC_SRC ; \
  git log --oneline --ancestry-path $MOZ_LIBWEBRTC_BASE^..$MOZ_GIT_RELEASE_BRANCH \
   | tail -2 | head -1 | awk '{print $1;}'`
}
