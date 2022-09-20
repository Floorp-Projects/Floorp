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
