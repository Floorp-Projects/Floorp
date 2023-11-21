#!/bin/bash

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

DEFAULT_CONFIG_PATH=dom/media/webrtc/third_party_build/default_config_env

# use the previous default_config_env to make sure any locally overriden
# settings don't interfere with the update process.
MOZ_CONFIG_PATH=$DEFAULT_CONFIG_PATH source dom/media/webrtc/third_party_build/use_config_env.sh

if [ "x" = "x$NEW_BUG_NUMBER" ]; then
  NEXT_MILESTONE=$(($MOZ_NEXT_LIBWEBRTC_MILESTONE+1))
  echo ""
  echo "NEW_BUG_NUMBER is not defined.  Please use the bug number created for"
  echo "updating the libwebrtc library to version $NEXT_MILESTONE."
  echo "For more information about creating this bug, see the first step of the"
  echo "prerequisites section of our wiki:"
  echo "https://wiki.mozilla.org/Media/WebRTC/libwebrtc_Update_Process#Prerequisites"
  echo ""
  echo "  NEW_BUG_NUMBER={v$NEXT_MILESTONE-bug-number} bash $0"
  echo ""
  exit
fi

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
