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

echo "MOZ_LIBWEBRTC_SRC: $MOZ_LIBWEBRTC_SRC"
echo "MOZ_LIBWEBRTC_BRANCH: $MOZ_LIBWEBRTC_BRANCH"
echo "MOZ_FASTFORWARD_BUG: $MOZ_FASTFORWARD_BUG"

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

Frequently using 'hg log' on the files listed with 'hg status' will give
information on commits that need to be added to the patch-stack.  After
identifying a commit or commit range from mercurial that should be added
to the git patch-stack, the following commands should help remedy the
situation:
  ./mach python $SCRIPT_DIR/extract-for-git.py {start-commit-sha}::{end-commit-sha}
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
