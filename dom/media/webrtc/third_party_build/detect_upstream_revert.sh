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

# If DEBUG_GEN is set all commands should be printed as they are executed
if [ ! "x$DEBUG_GEN" = "x" ]; then
  set -x
fi

if [ "x$MOZ_LIBWEBRTC_SRC" = "x" ]; then
  echo "MOZ_LIBWEBRTC_SRC is not defined, see README.md"
  exit
fi

if [ -d $MOZ_LIBWEBRTC_SRC ]; then
  echo "MOZ_LIBWEBRTC_SRC is $MOZ_LIBWEBRTC_SRC"
else
  echo "Path $MOZ_LIBWEBRTC_SRC is not found, see README.md"
  exit
fi

if [ "x$MOZ_LIBWEBRTC_BRANCH" = "x" ]; then
  echo "MOZ_LIBWEBRTC_BRANCH is not defined, see README.md"
  exit
fi

if [ "x$AUTO_FIX_REVERT_AS_NOOP" = "x" ]; then
  AUTO_FIX_REVERT_AS_NOOP="0"
fi

find_base_commit
find_next_commit

MOZ_LIBWEBRTC_COMMIT_MSG=`cd $MOZ_LIBWEBRTC_SRC ; \
git show --name-only --oneline $MOZ_LIBWEBRTC_NEXT_BASE \
 | head -1 | sed 's/[^ ]* //'`

echo "MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
echo "MOZ_LIBWEBRTC_NEXT_BASE: $MOZ_LIBWEBRTC_NEXT_BASE"
echo "MOZ_LIBWEBRTC_COMMIT_MSG: $MOZ_LIBWEBRTC_COMMIT_MSG"
export MOZ_LIBWEBRTC_REVERT_SHA=`cd $MOZ_LIBWEBRTC_SRC ; \
git log --oneline -r $MOZ_LIBWEBRTC_BASE..$MOZ_TARGET_UPSTREAM_BRANCH_HEAD \
 | grep -F "Revert \"$MOZ_LIBWEBRTC_COMMIT_MSG" \
 | tail -1 | awk '{print $1;}' || true`

if [ "x$MOZ_LIBWEBRTC_REVERT_SHA" == "x" ]; then
  echo "no revert commit summary detected in newer commits matching $MOZ_LIBWEBRTC_COMMIT_MSG"
  exit
fi

echo "found potential MOZ_LIBWEBRTC_REVERT_SHA: $MOZ_LIBWEBRTC_REVERT_SHA"
echo "checking commit message for 'This reverts commit' to confirm"
CONFIRM_SHA=`cd $MOZ_LIBWEBRTC_SRC ; \
git show $MOZ_LIBWEBRTC_REVERT_SHA \
 | awk '/This reverts commit/ { print $NF; }' \
 | tr -d '[:punct:]'`

if [ "x$CONFIRM_SHA" == "x" ]; then
  echo "no revert commit listed in commit message for $MOZ_LIBWEBRTC_REVERT_SHA"
  exit
fi

# convert to short sha
CONFIRM_SHA=`cd $MOZ_LIBWEBRTC_SRC ; \
git show --format='%h' --no-patch $CONFIRM_SHA`

if [ $MOZ_LIBWEBRTC_NEXT_BASE != $CONFIRM_SHA ]; then
  echo "revert sha ($MOZ_LIBWEBRTC_NEXT_BASE) does not match commit listed in commit summary ($CONFIRM_SHA)"
  exit
fi


if [ "x$AUTO_FIX_REVERT_AS_NOOP" = "x1" ]; then
  echo "AUTO_FIX_REVERT_AS_NOOP detected, fixing land/revert pair automatically"
  bash $SCRIPT_DIR/make_upstream_revert_noop.sh
  exit
fi

echo $"
The next upstream commit has a corresponding future \"Revert\" commit.

There are 2 common ways forward in this situation:
1. If you're relatively certain there will not be rebase conflicts in the
   github repo ($MOZ_LIBWEBRTC_SRC), simply run:
       SKIP_NEXT_REVERT_CHK=1 bash $SCRIPT_DIR/loop-ff.sh

2. The surer method for no rebase conflicts is to cherry-pick both the
   next commit, and the commit that reverts the next commit onto the
   bottom of our patch stack in github.  This pushes the likely rebase
   conflict into the future when the upstream fix is relanded, but
   ensures we only have to deal with the conflict once.  The following
   commands will add the necessary commits to the bottom of our patch
   stack in github, and leave indicator files in the home directory that
   help loop-ff know when to invoke special no-op commit handling:

       MOZ_LIBWEBRTC_BASE=$MOZ_LIBWEBRTC_BASE \\
       MOZ_LIBWEBRTC_NEXT_BASE=$MOZ_LIBWEBRTC_NEXT_BASE \\
       MOZ_LIBWEBRTC_REVERT_SHA=$MOZ_LIBWEBRTC_REVERT_SHA \\
       bash $SCRIPT_DIR/make_upstream_revert_noop.sh

       SKIP_NEXT_REVERT_CHK=1 bash $SCRIPT_DIR/loop-ff.sh
"
exit 1
