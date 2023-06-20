#!/bin/bash

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

# All commands should be printed as they are executed
# set -x

# After this point:
# * eE: All commands should succede.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succede.
set -eEuo pipefail

MOZ_BUILD_CHANGE_CNT=`hg status --include="**moz.build" third_party/libwebrtc | wc -l | tr -d " "`
echo "MOZ_BUILD_CHANGE_CNT: $MOZ_BUILD_CHANGE_CNT"
if [ "x$MOZ_BUILD_CHANGE_CNT" != "x0" ]; then
  CURRENT_COMMIT_SHA=`hg id -i | sed 's/+//'`
  COMMIT_DESC=`hg --config alias.log=log log -T '{desc|firstline}' -r $CURRENT_COMMIT_SHA`

  # since we have build file changes, touch the CLOBBER file
  cat CLOBBER | egrep "^#|^$" > CLOBBER.new
  mv CLOBBER.new CLOBBER
  echo "Modified build files in third_party/libwebrtc - $COMMIT_DESC" >> CLOBBER

  ADD_CNT=`hg status --include="**moz.build" -nu third_party/libwebrtc | wc -l | tr -d " "`
  DEL_CNT=`hg status --include="**moz.build" -nd third_party/libwebrtc | wc -l | tr -d " "`
  if [ "x$ADD_CNT" != "x0" ]; then
    hg status --include="**moz.build" -nu third_party/libwebrtc | xargs hg add
  fi
  if [ "x$DEL_CNT" != "x0" ]; then
    hg status --include="**moz.build" -nd third_party/libwebrtc | xargs hg rm
  fi

  hg commit -m \
    "$COMMIT_DESC - moz.build file updates" \
    --include="**moz.build" --include="CLOBBER" third_party/libwebrtc CLOBBER
fi

echo "Done in $0"
