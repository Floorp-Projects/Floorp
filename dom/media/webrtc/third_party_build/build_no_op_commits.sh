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

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

CURRENT_DIR=`pwd`
cd $MOZ_LIBWEBRTC_SRC

MANUAL_INTERVENTION_COMMIT_FILE="$TMP_DIR/manual_commits.txt"
rm -f $MANUAL_INTERVENTION_COMMIT_FILE

# Find the common commit between our previous work branch and trunk
CURRENT_RELEASE_BASE=`git merge-base branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM master`

# Write no-op files for the cherry-picked release branch commits.  For more
# details on what this is doing, see make_upstream_revert_noop.sh.
COMMITS=`git log -r $CURRENT_RELEASE_BASE..branch-heads/$MOZ_PRIOR_UPSTREAM_BRANCH_HEAD_NUM --format='%h'`
for commit in $COMMITS; do

  echo "Processing release branch commit $commit for no-op handling"

  # Don't process the commit if the commit message is missing the customary
  # line that shows which upstream commit is being cherry-picked.
  CNT=`git show $commit | grep "cherry picked from commit" | wc -l | tr -d " " || true`
  if [ $CNT != 1 ]; then
    # record the commit to list at the end of this script as
    # 'needing intervention'
    echo "    no cherry-pick info found, skipping commit $commit"
    echo "$commit" >> $MANUAL_INTERVENTION_COMMIT_FILE
    continue
  fi

  CHERRY_PICK_COMMIT=`git show $commit | grep "cherry picked from commit" | tr -d "()" | awk '{ print $5; }'`
  SHORT_SHA=`git show --name-only $CHERRY_PICK_COMMIT --format='%h' | head -1`
  echo "    commit $commit cherry-picks $SHORT_SHA"

  echo "We already cherry-picked this when we vendored $commit." \
  > $STATE_DIR/$SHORT_SHA.no-op-cherry-pick-msg

done

# This section checks for commits that may have been cherry-picked in more
# than one release branch.
TARGET_RELEASE_BASE=`git merge-base $MOZ_TARGET_UPSTREAM_BRANCH_HEAD master`
NEW_COMMITS=`git log -r $TARGET_RELEASE_BASE..$MOZ_TARGET_UPSTREAM_BRANCH_HEAD --format='%h'`

# Convert the files that we've already generated for no-op detection into
# something that we can use as a regular expression for searching.
KNOWN_NO_OP_COMMITS=`cd $STATE_DIR ; \
  ls *.no-op-cherry-pick-msg \
  | sed 's/\.no-op-cherry-pick-msg//' \
  | paste -sd '|' /dev/stdin`

for commit in $NEW_COMMITS; do

  echo "Processing next release branch commit $commit for no-op handling"

  # Don't process the commit if the commit message is missing the customary
  # line that shows which upstream commit is being cherry-picked.
  CNT=`git show $commit | grep "cherry picked from commit" | wc -l | tr -d " " || true`
  if [ $CNT != 1 ]; then
    # record the commit to list at the end of this script as
    # 'needing intervention'
    echo "    no cherry-pick info found, skipping commit $commit"
    echo "$commit" >> $MANUAL_INTERVENTION_COMMIT_FILE
    continue
  fi

  CHERRY_PICK_COMMIT=`git show $commit | grep "cherry picked from commit" | tr -d "()" | awk '{ print $5; }'`
  SHORT_SHA=`git show --name-only $CHERRY_PICK_COMMIT --format='%h' | head -1`

  # The trick here is that we only want to include no-op processing for the
  # commits that appear both here _and_ in the previous release's cherry-pick
  # commits.  We check the known list of no-op commits to see if it was
  # cherry picked in the previous release branch and then create another
  # file for the new release branch commit that will ultimately be a no-op.
  if [[ "$SHORT_SHA" =~ ^($KNOWN_NO_OP_COMMITS)$ ]]; then
    echo "    commit $commit cherry-picks $SHORT_SHA"
    cp $STATE_DIR/$SHORT_SHA.no-op-cherry-pick-msg $STATE_DIR/$commit.no-op-cherry-pick-msg
  fi

done

if [ ! -f $MANUAL_INTERVENTION_COMMIT_FILE ]; then
  echo "No commits require manual intervention"
  exit
fi

echo $"
Each of the following commits requires manual intervention to
verify the source of the cherry-pick or there may be errors
reported during the fast-forward processing.  Without this
intervention, the common symptom is that the vendored commit
file count (0) will not match the upstream commit file count.
"

for commit in `cat $MANUAL_INTERVENTION_COMMIT_FILE`; do
  SUMMARY=`git show --oneline --name-only $commit | head -1`
  echo "  '$SUMMARY'"
done

echo $"
To manually create the no-op tracking files needed,
run the following command (in bash) for each commit in question:
  ( export FUTURE_UPSTREAM_COMMIT=\"{short-sha-of-upstream-commit}\" ; \\
    export ALREADY_USED_COMMIT=\"{short-sha-of-already-used-commit}\" ; \\
    echo \"We already cherry-picked this when we vendored \$ALREADY_USED_COMMIT.\" \\
    > $STATE_DIR/\$FUTURE_UPSTREAM_COMMIT.no-op-cherry-pick-msg )
"
