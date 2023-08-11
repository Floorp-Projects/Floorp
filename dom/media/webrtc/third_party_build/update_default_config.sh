#!/bin/bash

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

if [ "x" = "x$NEW_BUG_NUMBER" ]; then
  echo "NEW_BUG_NUMBER is not defined.  You should probably have a new bug"
  echo "number defined for the next fast-forward update.  Then do:"
  echo "  NEW_BUG_NUMBER={new-bug-number} bash $0"
  exit
fi

DEFAULT_CONFIG_PATH=dom/media/webrtc/third_party_build/default_config_env

# use the previous default_config_env to make sure any locally overriden
# settings don't interfere with the update process.
MOZ_CONFIG_PATH=$DEFAULT_CONFIG_PATH source dom/media/webrtc/third_party_build/use_config_env.sh

if [ "x$MOZ_NEXT_LIBWEBRTC_MILESTONE" = "x" ]; then
  echo "MOZ_NEXT_LIBWEBRTC_MILESTONE is not defined, see README.md"
  exit
fi

if [ "x$MOZ_NEXT_FIREFOX_REL_TARGET" = "x" ]; then
  echo "MOZ_NEXT_FIREFOX_REL_TARGET is not defined, see README.md"
  exit
fi


# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

ERROR_HELP=$"
An error has occurred running $SCRIPT_DIR/write_default_config.py
"
MOZCONFIG=dom/media/webrtc/third_party_build/default_mozconfig \
  ./mach python $SCRIPT_DIR/write_default_config.py \
  --prior-bug-number $MOZ_FASTFORWARD_BUG \
  --bug-number $NEW_BUG_NUMBER \
  --milestone $MOZ_NEXT_LIBWEBRTC_MILESTONE \
  --release-target $MOZ_NEXT_FIREFOX_REL_TARGET \
  --output-path $SCRIPT_DIR/default_config_env

# source our newly updated default_config_env so we can use the new settings
# to automatically commit the updated file.
source $DEFAULT_CONFIG_PATH

hg commit -m \
  "Bug $MOZ_FASTFORWARD_BUG - updated default_config_env for v$MOZ_NEXT_LIBWEBRTC_MILESTONE" \
  $DEFAULT_CONFIG_PATH
