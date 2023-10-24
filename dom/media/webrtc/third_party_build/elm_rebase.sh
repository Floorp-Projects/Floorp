#!/bin/bash

# This script exists to help with the rebase process on elm.  It rebases
# each patch individually to make it easier to fix rebase conflicts
# without jeopardizing earlier, sucessfully rebased commits. In order to
# limit rebase conflicts around generated moz.build files, it regenerates
# moz.build file commits.  It also ensures any commits with 'FLOAT' in the
# commit summary are pushed to the top of the fast-forward stack to help
# the sheriffs more easily merge our commit stack from elm to moz-central.
#
# Occasionally, there will be upstream vendored commits that break the
# build file generation with follow on commits that fix that error. In
# order to allow the rebase process to work more smoothly, it is possible
# to annotate a commit with the string '(skip-generation)' and normal
# build file generation (detected with changes to BUILD.gn files) is
# disabled for that commit.  The script outputs instructions for handling
# this situation.
#
# Note: the very first rebase operation may require some manual
# intervention. The user will need to provide, at minimum, the first
# commit of the fast-forward stack if the script is unable to determine
# it automatically.  Example:
#   MOZ_BOTTOM_FF=30f0afb7e4c5 \
#   bash dom/media/webrtc/third_party_build/elm_rebase.sh
#
# Assumes the top of the fast-forward stack to rebase is the current revision,
# ".".

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!"
  echo "$ERROR_HELP"
}
ERROR_HELP=""

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh

GENERATION_ERROR=$"
Generating build files has failed.  The most common reason for this
failure is that the current commit has an upcoming '(fix-xxxxxx)' commit
that will then allow the build file generation to complete.  If the
current situation seems to fit that pattern, adding a line with
'(skip-generation)' to the commit message will ensure that future rebase
operations do not attempt to generate build files for this commit.  It may
be as simple as running the following commands:
  HGPLAIN=1 hg log -T '{desc}' -r tip > $TMP_DIR/commit_message.txt
  ed -s $TMP_DIR/commit_message.txt <<< $'3i\n(skip-generation)\n\n.\nw\nq'
  hg commit --amend -l $TMP_DIR/commit_message.txt
  bash $0
"
COMMIT_LIST_FILE=$TMP_DIR/rebase-commit-list.txt
export HGPLAIN=1

# After this point:
# * eE: All commands should succeed.
# * o pipefail: All stages of all pipes should succeed.
set -eEo pipefail

if [ -f $STATE_DIR/rebase_resume_state ]; then
  source $STATE_DIR/rebase_resume_state
else

  if [ "x" == "x$MOZ_TOP_FF" ]; then
    MOZ_TOP_FF=`hg log -r . -T"{node|short}"`

    ERROR_HELP=$"
The topmost commit to be rebased is not in the public phase. Should it be
pushed to elm first? If this is intentional, please rerun the command and pass
it in explicitly:
  MOZ_TOP_FF=$MOZ_TOP_FF bash $0
"
    if [[ $(hg phase -r .) != *public ]]; then
      echo "$ERROR_HELP"
      exit 1
    fi
    ERROR_HELP=""

    ERROR_HELP=$"
The topmost commit to be rebased is public but has descendants. If those
descendants should not be rebased, please rerun the command and pass the commit
in explicitly:
  MOZ_TOP_FF=$MOZ_TOP_FF bash $0
"
    if [ "x" != "x$(hg log -r 'descendants(.) and !.' -T'{node|short}')" ]; then
      echo "$ERROR_HELP"
      exit 1
    fi
    ERROR_HELP=""
  fi

  hg pull central

  ERROR_HELP=$"
Automatically determining the bottom (earliest) commit of the fast-forward
stack has failed.  Please provide the bottom commit of the fast-forward
stack.  The bottom commit means the commit following the most recent
mozilla-central commit.  This could be the sha of the .arcconfig commit
if it is the bottom commit.
That command looks like:
  MOZ_BOTTOM_FF={base-sha} bash $0
"
  if [ "x" == "x$MOZ_BOTTOM_FF" ]; then
    # Finds the common ancestor between our top fast-forward commit and
    # mozilla-central using:
    #    ancestor($MOZ_TOP_FF, central)
    MOZ_OLD_CENTRAL=`hg id --id --rev "ancestor($MOZ_TOP_FF, central)"`
    # Using that ancestor and $MOZ_TOP_FF as a range, find the commit _after_
    # the the common commit using limit(range, 1, 1) which gives the first
    # commit of the range, offset by one commit.
    MOZ_BOTTOM_FF=`hg id --id --rev "limit($MOZ_OLD_CENTRAL::$MOZ_TOP_FF, 1, 1)"`
  fi
  if [ "x" == "x$MOZ_BOTTOM_FF" ]; then
    echo "No value found for the bottom commit of the fast-forward commit stack."
    echo "$ERROR_HELP"
    exit 1
  fi
  ERROR_HELP=""

  # After this point:
  # * eE: All commands should succeed.
  # * u: All variables should be defined before use.
  # * o pipefail: All stages of all pipes should succeed.
  set -eEuo pipefail

  MOZ_NEW_CENTRAL=`hg log -r central -T"{node|short}"`

  echo "bottom of fast-foward tree is $MOZ_BOTTOM_FF"
  echo "top of fast-forward tree (webrtc-fast-forward) is $MOZ_TOP_FF"
  echo "new target for elm rebase $MOZ_NEW_CENTRAL (tip of moz-central)"

  hg log -T '{rev}:{node|short} {desc|firstline}\n' \
      -r $MOZ_BOTTOM_FF::$MOZ_TOP_FF > $COMMIT_LIST_FILE

  # move all FLOAT lines to end of file, and delete the "empty" tilde line
  # line at the beginning
  ed -s $COMMIT_LIST_FILE <<< $'g/- FLOAT -/m$\ng/^~$/d\nw\nq'

  MOZ_BOOKMARK=`date "+webrtc-fast-forward-%Y-%m-%d--%H-%M"`
  hg bookmark -r elm $MOZ_BOOKMARK

  hg update $MOZ_NEW_CENTRAL

  # pre-work is complete, let's write out a temporary config file that allows
  # us to resume
  echo $"export MOZ_BOTTOM_FF=$MOZ_BOTTOM_FF
export MOZ_TOP_FF=$MOZ_TOP_FF
export MOZ_OLD_CENTRAL=$MOZ_OLD_CENTRAL
export MOZ_NEW_CENTRAL=$MOZ_NEW_CENTRAL
export MOZ_BOOKMARK=$MOZ_BOOKMARK
" > $STATE_DIR/rebase_resume_state
fi # if [ -f $STATE_DIR/rebase_resume_state ]; then ; else

# grab all commits
COMMITS=`cat $COMMIT_LIST_FILE | awk '{print $1;}'`

echo -n "Commits: "
for commit in $COMMITS; do
echo -n "$commit "
done
echo ""

for commit in $COMMITS; do
  echo "Processing $commit"
  FULL_COMMIT_LINE=`head -1 $COMMIT_LIST_FILE`

  function remove_commit () {
    echo "Removing from list '$FULL_COMMIT_LINE'"
    ed -s $COMMIT_LIST_FILE <<< $'1d\nw\nq'
  }

  IS_BUILD_COMMIT=`hg log -T '{desc|firstline}' -r $commit \
                   | grep "file updates" | wc -l | tr -d " " || true`
  echo "IS_BUILD_COMMIT: $IS_BUILD_COMMIT"
  if [ "x$IS_BUILD_COMMIT" != "x0" ]; then
    echo "Skipping $commit:"
    hg log -T '{desc|firstline}' -r $commit
    remove_commit
    continue
  fi

  IS_SKIP_GEN_COMMIT=`hg log --verbose \
                         -r $commit \
                      | grep "skip-generation" | wc -l | tr -d " " || true`
  echo "IS_SKIP_GEN_COMMIT: $IS_SKIP_GEN_COMMIT"

  echo "Generate patch for: $commit"
  hg export -r $commit > $TMP_DIR/rebase.patch

  echo "Import patch for $commit"
  hg import $TMP_DIR/rebase.patch || \
  ( hg log -T '{desc}' -r $commit > $TMP_DIR/rebase_commit_message.txt ; \
    remove_commit ; \
    echo "Error importing: '$FULL_COMMIT_LINE'" ; \
    echo "Please fix import errors, then:" ; \
    echo "  hg commit -l $TMP_DIR/rebase_commit_message.txt" ; \
    echo "  bash $0" ; \
    exit 1 )

  remove_commit

  if [ "x$IS_SKIP_GEN_COMMIT" != "x0" ]; then
    echo "Skipping build generation for $commit"
    continue
  fi

  MODIFIED_BUILD_RELATED_FILE_CNT=`hg diff -c tip --stat \
      --include 'third_party/libwebrtc/**BUILD.gn' \
      --include 'third_party/libwebrtc/webrtc.gni' \
      --include 'dom/media/webrtc/third_party_build/gn-configs/webrtc.json' \
      | wc -l | tr -d " "`
  echo "MODIFIED_BUILD_RELATED_FILE_CNT: $MODIFIED_BUILD_RELATED_FILE_CNT"
  if [ "x$MODIFIED_BUILD_RELATED_FILE_CNT" != "x0" ]; then
    echo "Regenerate build files"
    ./mach python python/mozbuild/mozbuild/gn_processor.py \
        dom/media/webrtc/third_party_build/gn-configs/webrtc.json || \
    ( echo "$GENERATION_ERROR" ; exit 1 )

    MOZ_BUILD_CHANGE_CNT=`hg status third_party/libwebrtc \
        --include 'third_party/libwebrtc/**moz.build' | wc -l | tr -d " "`
    if [ "x$MOZ_BUILD_CHANGE_CNT" != "x0" ]; then
      bash dom/media/webrtc/third_party_build/commit-build-file-changes.sh
      NEWEST_COMMIT=`hg log -T '{desc|firstline}' -r tip`
      echo "NEWEST_COMMIT: $NEWEST_COMMIT"
      echo "NEWEST_COMMIT: $NEWEST_COMMIT" >> $LOG_DIR/rebase-build-changes-commits.log
    fi
    echo "Done generating build files"
  fi

  echo "Done processing $commit"
done

rm $STATE_DIR/rebase_resume_state

# This is blank in case no changes have been made in third_party/libwebrtc
# since the previous rebase (or original elm reset).
PATCH_STACK_FIXUP=""

echo "Checking for new mercurial changes in third_party/libwebrtc"
FIXUP_INSTRUCTIONS=$"
Mercurial changes in third_party/libwebrtc since the last rebase have been
detected (using the verify_vendoring.sh script).  Running the following
commands should remedy the situation:

  ./mach python $SCRIPT_DIR/extract-for-git.py $MOZ_OLD_CENTRAL::$MOZ_NEW_CENTRAL
  mv mailbox.patch $MOZ_LIBWEBRTC_SRC
  (cd $MOZ_LIBWEBRTC_SRC && \\
   git am mailbox.patch)
  bash $SCRIPT_DIR/verify_vendoring.sh

When verify_vendoring.sh is successful, run the following in bash:
  (source $SCRIPT_DIR/use_config_env.sh ;
   ./mach python $SCRIPT_DIR/save_patch_stack.py \\
    --repo-path $MOZ_LIBWEBRTC_SRC \\
    --target-branch-head $MOZ_TARGET_UPSTREAM_BRANCH_HEAD  \\
    --separate-commit-bug-number $MOZ_FASTFORWARD_BUG )
"
bash $SCRIPT_DIR/verify_vendoring.sh &> $LOG_DIR/log-verify.txt || PATCH_STACK_FIXUP="$FIXUP_INSTRUCTIONS"
echo "Done checking for new mercurial changes in third_party/libwebrtc"

REMAINING_STEPS=$"
The rebase process is complete.  The following steps must be completed manually:
$PATCH_STACK_FIXUP
  ./mach bootstrap --application=browser --no-system-changes && \\
  ./mach build && \\
  hg push -r tip --force && \\
  hg push -B $MOZ_BOOKMARK
"
echo "$REMAINING_STEPS"
