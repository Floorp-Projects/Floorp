#!/bin/bash

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'echo "*** ERROR *** $? $LINENO $0 did not complete successfully!"' ERR 

# All commands should be printed as they are executed
# set -x

# After this point:
# * eE: All commands should succede.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succede.
set -eEuo pipefail

MOZ_BUILD_CHANGE_CNT=`hg status third_party/libwebrtc | wc -l | tr -d " "`
echo "MOZ_BUILD_CHANGE_CNT: $MOZ_BUILD_CHANGE_CNT"
if [ "x$MOZ_BUILD_CHANGE_CNT" != "x0" ]; then
  CURRENT_COMMIT_SHA=`hg id -i | sed 's/+//'`
  COMMIT_DESC=`hg --config alias.log=log log -T '{desc|firstline}' -r $CURRENT_COMMIT_SHA`

  # since we have build file changes, touch the CLOBBER file
  cat CLOBBER | egrep "^#|^$" > CLOBBER.new
  mv CLOBBER.new CLOBBER
  echo "Modified build files in third_party/libwebrtc - $COMMIT_DESC" >> CLOBBER

  ADD_CNT=`hg status -nu third_party/libwebrtc | wc -l`
  DEL_CNT=`hg status -nd third_party/libwebrtc | wc -l`
  if [ "x$ADD_CNT" != "x0" ]; then
    hg status -nu third_party/libwebrtc | xargs hg add
  fi
  if [ "x$DEL_CNT" != "x0" ]; then
    hg status -nd third_party/libwebrtc | xargs hg rm
  fi

  hg commit -m \
    "$COMMIT_DESC - moz.build file updates" \
    third_party/libwebrtc CLOBBER
fi

echo "Done in $0"
