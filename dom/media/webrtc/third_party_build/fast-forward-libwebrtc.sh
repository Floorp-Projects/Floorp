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

if [ "x$HANDLE_NOOP_COMMIT" = "x" ]; then
  HANDLE_NOOP_COMMIT=""
fi

RESUME=""
if [ -f $STATE_DIR/resume_state ]; then
  RESUME=`tail -1 $STATE_DIR/resume_state`
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

echo "looking for $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg"
if [ -f $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg ]; then
  echo "***"
  echo "*** detected special commit msg, setting HANDLE_NOOP_COMMIT"
  echo "***"
  HANDLE_NOOP_COMMIT="1"
fi

UPSTREAM_ADDED_FILES=""

# Grab the filtered changes from git based on what we vendor.
FILTERED_GIT_CHANGES=`./mach python $SCRIPT_DIR/filter_git_changes.py \
   --repo-path $MOZ_LIBWEBRTC_SRC --commit-sha $MOZ_LIBWEBRTC_NEXT_BASE`

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

echo "     MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
echo "MOZ_LIBWEBRTC_NEXT_BASE: $MOZ_LIBWEBRTC_NEXT_BASE"
echo "HANDLE_NOOP_COMMIT: $HANDLE_NOOP_COMMIT"
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
#"rebase_mozlibwebrtc_stack help for rebase failure"
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

function vendor_off_next_commit {
  echo "-------"
  echo "------- Vendor $MOZ_LIBWEBRTC_BRANCH from $MOZ_LIBWEBRTC_SRC"
  echo "-------"
  ./mach python $SCRIPT_DIR/vendor-libwebrtc.py \
            --from-local $MOZ_LIBWEBRTC_SRC \
            --commit $MOZ_LIBWEBRTC_BRANCH \
            libwebrtc
}

# The vendoring script (called above in vendor_off_next_commit) replaces
# the entire third_party/libwebrtc directory, which effectively removes
# all the generated moz.build files.  It is easier (less error prone),
# to revert only those missing moz.build files rather than attempt to
# rebuild them because the rebuild may need json updates to work properly.
function regen_mozbuild_files {
  echo "-------"
  echo "------- Restore moz.build files from repo"
  echo "-------"
  hg revert --include "third_party/libwebrtc/**moz.build" \
    third_party/libwebrtc &> $LOG_DIR/log-regen-mozbuild-files.txt
}

function add_new_upstream_files {
  if [ "x$HANDLE_NOOP_COMMIT" == "x1" ]; then
    return
  fi
  UPSTREAM_ADDED_FILES=`echo "$FILTERED_GIT_CHANGES" | grep "^A" \
      | awk '{print $2;}' || true`
  if [ "x$UPSTREAM_ADDED_FILES" != "x" ]; then
    echo "-------"
    echo "------- Add new upstream files"
    echo "-------"
    (cd third_party/libwebrtc && hg add $UPSTREAM_ADDED_FILES)
    echo "$UPSTREAM_ADDED_FILES" &> $LOG_DIR/log-new-upstream-files.txt
  fi
}

function remove_deleted_upstream_files {
  if [ "x$HANDLE_NOOP_COMMIT" == "x1" ]; then
    return
  fi
  UPSTREAM_DELETED_FILES=`echo "$FILTERED_GIT_CHANGES" | grep "^D" \
      | awk '{print $2;}' || true`
  if [ "x$UPSTREAM_DELETED_FILES" != "x" ]; then
    echo "-------"
    echo "------- Remove deleted upstream files"
    echo "-------"
    (cd third_party/libwebrtc && hg rm $UPSTREAM_DELETED_FILES)
    echo "$UPSTREAM_DELETED_FILES" &> $LOG_DIR/log-deleted-upstream-files.txt
  fi
}

function handle_renamed_upstream_files {
  if [ "x$HANDLE_NOOP_COMMIT" == "x1" ]; then
    return
  fi
  UPSTREAM_RENAMED_FILES=`echo "$FILTERED_GIT_CHANGES" | grep "^R" \
      | awk '{print $2 " " $3;}' || true`
  if [ "x$UPSTREAM_RENAMED_FILES" != "x" ]; then
    echo "-------"
    echo "------- Handle renamed upstream files"
    echo "-------"
    (cd third_party/libwebrtc && echo "$UPSTREAM_RENAMED_FILES" | while read line; do hg rename --after $line; done)
    echo "$UPSTREAM_RENAMED_FILES" &> $LOG_DIR/log-renamed-upstream-files.txt
  fi
}

if [ $SKIP_TO = "run" ]; then
  echo "resume2" > $STATE_DIR/resume_state
  rebase_mozlibwebrtc_stack;
fi

if [ $SKIP_TO = "resume2" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume3" > $STATE_DIR/resume_state
  vendor_off_next_commit;
fi

if [ $SKIP_TO = "resume3" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume4" > $STATE_DIR/resume_state
  regen_mozbuild_files;
fi

if [ $SKIP_TO = "resume4" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume5" > $STATE_DIR/resume_state
  remove_deleted_upstream_files;
fi

if [ $SKIP_TO = "resume5" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume6" > $STATE_DIR/resume_state
  add_new_upstream_files;
fi

if [ $SKIP_TO = "resume6" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume7" > $STATE_DIR/resume_state
  handle_renamed_upstream_files;
fi

echo "" > $STATE_DIR/resume_state
echo "-------"
echo "------- Commit vendored changes from $MOZ_LIBWEBRTC_NEXT_BASE"
echo "-------"
UPSTREAM_SHA=`cd $MOZ_LIBWEBRTC_SRC && \
    git show --name-only $MOZ_LIBWEBRTC_NEXT_BASE \
    | grep "^commit " | awk '{ print $NF }'`
echo "Bug $MOZ_FASTFORWARD_BUG - Vendor libwebrtc from $MOZ_LIBWEBRTC_NEXT_BASE" \
    > $TMP_DIR/commit_msg.txt
echo "" >> $TMP_DIR/commit_msg.txt
if [ -f $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg ]; then
  cat $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg >> $TMP_DIR/commit_msg.txt
  echo "" >> $TMP_DIR/commit_msg.txt
  rm $STATE_DIR/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg
fi
echo "Upstream commit: https://webrtc.googlesource.com/src/+/$UPSTREAM_SHA" >> $TMP_DIR/commit_msg.txt
(cd $MOZ_LIBWEBRTC_SRC && \
git show --name-only $MOZ_LIBWEBRTC_NEXT_BASE | grep "^ ") >> $TMP_DIR/commit_msg.txt

hg commit -l $TMP_DIR/commit_msg.txt third_party/libwebrtc
