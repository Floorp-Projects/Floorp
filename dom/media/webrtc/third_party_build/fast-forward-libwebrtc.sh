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

RESUME_FILE=$STATE_DIR/fast_forward.resume
RESUME=""
if [ -f $RESUME_FILE ]; then
  RESUME=`tail -1 $RESUME_FILE`
fi

GIT_IS_REBASING=`cd $MOZ_LIBWEBRTC_SRC && git status | grep "interactive rebase in progress" | wc -l | tr -d " " || true`
if [ "x$GIT_IS_REBASING" != "x0" ]; then
  echo "There is currently a git rebase operation in progress at $MOZ_LIBWEBRTC_SRC."
  echo "Please resolve the rebase before attempting to continue the fast-forward"
  echo "operation."
  exit 1
fi

if [ "x$RESUME" = "x" ]; then
  SKIP_TO="run"
  # Check for modified files and abort if present.
  MODIFIED_FILES=`hg status --exclude "third_party/libwebrtc/**.orig" third_party/libwebrtc`
  if [ "x$MODIFIED_FILES" = "x" ]; then
    # Completely clean the mercurial checkout before proceeding
    hg update -C -r .
    hg purge
  else
    echo "There are modified files in the checkout. Cowardly aborting!"
    echo "$MODIFIED_FILES"
    exit 1
  fi
else
  SKIP_TO=$RESUME
  hg revert -C third_party/libwebrtc/README.moz-ff-commit &> /dev/null
fi

find_base_commit
find_next_commit

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

echo "     MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
echo "MOZ_LIBWEBRTC_NEXT_BASE: $MOZ_LIBWEBRTC_NEXT_BASE"
echo " RESUME: $RESUME"
echo "SKIP_TO: $SKIP_TO"

echo "-------"
echo "------- Write cmd-line to third_party/libwebrtc/README.moz-ff-commit"
echo "-------"
echo "# MOZ_LIBWEBRTC_SRC=$MOZ_LIBWEBRTC_SRC MOZ_LIBWEBRTC_BRANCH=$MOZ_LIBWEBRTC_BRANCH bash $0" \
    >> third_party/libwebrtc/README.moz-ff-commit

echo "-------"
echo "------- Write new-base to last line of third_party/libwebrtc/README.moz-ff-commit"
echo "-------"
echo "# base of lastest vendoring" >> third_party/libwebrtc/README.moz-ff-commit
echo "$MOZ_LIBWEBRTC_NEXT_BASE" >> third_party/libwebrtc/README.moz-ff-commit

REBASE_HELP=$"
The rebase operation onto $MOZ_LIBWEBRTC_NEXT_BASE has failed.  Please
resolve all the rebase conflicts.  To fix this issue, you will need to
jump to the github repo at $MOZ_LIBWEBRTC_SRC .
When the github rebase is complete, re-run the script to resume the
fast-forward process.
"
function rebase_mozlibwebrtc_stack {
  echo "-------"
  echo "------- Rebase $MOZ_LIBWEBRTC_BRANCH to $MOZ_LIBWEBRTC_NEXT_BASE"
  echo "-------"
  ERROR_HELP=$REBASE_HELP
  ( cd $MOZ_LIBWEBRTC_SRC && \
    git checkout -q $MOZ_LIBWEBRTC_BRANCH && \
    git rebase $MOZ_LIBWEBRTC_NEXT_BASE \
    &> $LOG_DIR/log-rebase-moz-libwebrtc.txt \
  )
  ERROR_HELP=""
}

function write_commit_message_file {
  echo "-------"
  echo "------- Write commit message file ($TMP_DIR/commit_msg.txt)"
  echo "-------"
  UPSTREAM_LONG_SHA=`cd $MOZ_LIBWEBRTC_SRC && \
      git show --format='%H' --no-patch $MOZ_LIBWEBRTC_NEXT_BASE`
  echo "Bug $MOZ_FASTFORWARD_BUG - Vendor libwebrtc from $MOZ_LIBWEBRTC_NEXT_BASE" \
      > $TMP_DIR/commit_msg.txt
  echo "" >> $TMP_DIR/commit_msg.txt
  if [ -f $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg ]; then
    cat $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg >> $TMP_DIR/commit_msg.txt
    echo "" >> $TMP_DIR/commit_msg.txt
  fi
  echo "Upstream commit: https://webrtc.googlesource.com/src/+/$UPSTREAM_LONG_SHA" >> $TMP_DIR/commit_msg.txt
  (cd $MOZ_LIBWEBRTC_SRC && \
  git show --name-only $MOZ_LIBWEBRTC_NEXT_BASE | grep "^ ") >> $TMP_DIR/commit_msg.txt
}

if [ $SKIP_TO = "run" ]; then
  echo "resume2" > $RESUME_FILE
  rebase_mozlibwebrtc_stack;
fi

if [ $SKIP_TO = "resume2" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume3" > $RESUME_FILE
  write_commit_message_file;
fi

if [ $SKIP_TO = "resume3" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  ./mach python dom/media/webrtc/third_party_build/vendor_and_commit.py \
      --repo-path $MOZ_LIBWEBRTC_SRC \
      --script-path $SCRIPT_DIR \
      --commit-msg-path $TMP_DIR/commit_msg.txt \
      --commit-sha $MOZ_LIBWEBRTC_NEXT_BASE
fi

echo "" > $RESUME_FILE

# now that we've committed the vendored code, we can delete the
# no-op commit tracking file if it exists.
if [ -f $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg ]; then
  rm $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg
fi
