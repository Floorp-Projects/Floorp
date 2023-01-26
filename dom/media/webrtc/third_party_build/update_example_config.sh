#!/bin/bash

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh

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
An error has occurred running $SCRIPT_DIR/write_example_config.py
"
./mach python $SCRIPT_DIR/write_example_config.py \
  --milestone $MOZ_NEXT_LIBWEBRTC_MILESTONE \
  --release-target $MOZ_NEXT_FIREFOX_REL_TARGET \
  > $SCRIPT_DIR/example_config_env
