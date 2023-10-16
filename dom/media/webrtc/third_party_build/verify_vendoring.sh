#!/bin/bash

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh
export HGPLAIN=1

echo "MOZ_LIBWEBRTC_SRC: $MOZ_LIBWEBRTC_SRC"
echo "MOZ_LIBWEBRTC_BRANCH: $MOZ_LIBWEBRTC_BRANCH"
echo "MOZ_FASTFORWARD_BUG: $MOZ_FASTFORWARD_BUG"

LAST_PATCHSTACK_UPDATE_COMMIT=`hg log --template "{node|short} {desc|firstline}\n" \
    --include "third_party/libwebrtc/moz-patch-stack/*.patch" | head -1`
echo "LAST_PATCHSTACK_UPDATE_COMMIT: $LAST_PATCHSTACK_UPDATE_COMMIT"

TIP_SHA=`hg id -r tip | awk '{ print $1; }'`
echo "TIP_SHA: $TIP_SHA"

LAST_PATCHSTACK_UPDATE_COMMIT_SHA=`echo $LAST_PATCHSTACK_UPDATE_COMMIT \
    | awk '{ print $1; }'`
echo "LAST_PATCHSTACK_UPDATE_COMMIT_SHA: $LAST_PATCHSTACK_UPDATE_COMMIT_SHA"

if [ "x$TIP_SHA" == "x$LAST_PATCHSTACK_UPDATE_COMMIT_SHA" ]; then
  COMMIT_AFTER_PATCHSTACK_UPDATE="{start-commit-sha}"
else
  COMMIT_AFTER_PATCHSTACK_UPDATE=`hg id -r $LAST_PATCHSTACK_UPDATE_COMMIT_SHA~-1 || true`
  echo "COMMIT_AFTER_PATCHSTACK_UPDATE: $COMMIT_AFTER_PATCHSTACK_UPDATE"
fi

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

./mach python $SCRIPT_DIR/vendor-libwebrtc.py \
        --from-local $MOZ_LIBWEBRTC_SRC \
        --commit $MOZ_LIBWEBRTC_BRANCH \
        libwebrtc

hg revert -q \
   --include "third_party/libwebrtc/**moz.build" \
   --include "third_party/libwebrtc/README.mozilla" \
   third_party/libwebrtc

ERROR_HELP=$"
***
There are changes detected after vendoring libwebrtc from our local copy
of the git repo containing our patch-stack:
$MOZ_LIBWEBRTC_SRC

Typically this is due to changes made in mercurial to files residing
under third_party/libwebrtc that have not been reflected in
moz-libwebrtc git repo's patch-stack.

The following commands should help remedy the situation:
  ./mach python $SCRIPT_DIR/extract-for-git.py $COMMIT_AFTER_PATCHSTACK_UPDATE::tip
  mv mailbox.patch $MOZ_LIBWEBRTC_SRC
  (cd $MOZ_LIBWEBRTC_SRC && \\
   git am mailbox.patch)

After adding the new changes from mercurial to the moz-libwebrtc
patch stack, you should re-run this command to verify vendoring:
  bash $0
"
FILE_CHANGE_CNT=`hg status third_party/libwebrtc | wc -l | tr -d " "`
if [ "x$FILE_CHANGE_CNT" != "x0" ]; then
  echo "$ERROR_HELP"
  exit 1
fi


echo "Done - vendoring has been verified."
