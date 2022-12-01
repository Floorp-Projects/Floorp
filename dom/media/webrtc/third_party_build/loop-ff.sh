#!/bin/bash

# file for logging loop script output
LOOP_OUTPUT_LOG=~/log-loop-ff.txt

function show_error_msg()
{
  echo "*** ERROR *** $? line $1 $0 did not complete successfully!" 2>&1| tee -a $LOOP_OUTPUT_LOG
  echo "$ERROR_HELP"
}

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'show_error_msg $LINENO' ERR

source dom/media/webrtc/third_party_build/use_config_env.sh

# If DEBUG_LOOP_FF is set all commands should be printed as they are executed
if [ ! "x$DEBUG_LOOP_FF" = "x" ]; then
  set -x
fi

if [ "x$MOZ_LIBWEBRTC_SRC" = "x" ]; then
  echo "MOZ_LIBWEBRTC_SRC is not defined, see README.md"
  exit
fi

if [ ! -d $MOZ_LIBWEBRTC_SRC ]; then
  echo "Path $MOZ_LIBWEBRTC_SRC is not found, see README.md"
  exit
fi

if [ "x$MOZ_LIBWEBRTC_COMMIT" = "x" ]; then
  echo "MOZ_LIBWEBRTC_COMMIT is not defined, see README.md"
  exit
fi

if [ "x$MOZ_STOP_AFTER_COMMIT" = "x" ]; then
  MOZ_STOP_AFTER_COMMIT=""
  echo "No MOZ_STOP_AFTER_COMMIT variable defined - attempting unlimited advance"
fi

if [ "x$SKIP_NEXT_REVERT_CHK" = "x" ]; then
  SKIP_NEXT_REVERT_CHK="0"
fi

MOZ_CHANGED=0
GIT_CHANGED=0
ERROR_HELP=""
HANDLE_NOOP_COMMIT=""

# After this point:
# * eE: All commands should succeed.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succeed.
set -eEuo pipefail

# start a new log with every run of this script
rm -f $LOOP_OUTPUT_LOG
# make sure third_party/libwebrtc/README.moz-ff-commit is the committed version
# so we properly determine MOZ_LIBWEBRTC_BASE and MOZ_LIBWEBRTC_NEXT_BASE
# in the loop below
hg revert -C third_party/libwebrtc/README.moz-ff-commit &> /dev/null

# check for a resume situation from fast-forward-libwebrtc.sh
RESUME=""
if [ -f log_resume.txt ]; then
  RESUME=`tail -1 log_resume.txt`
fi

ERROR_HELP=$"
It appears that initial vendoring verification has failed.
- If you have never run the fast-forward process before, you may need to
  prepare the github repository by running prep_repo.sh.
- If you have previously run loop-ff.sh successfully, there may be a new
  change to third_party/libwebrtc that should be extracted and added to
  the patch stack in github.  It may be as easy as running:
      python3 dom/media/webrtc/third_party_build/extract-for-git.py tip::tip
      mv mailbox.patch $MOZ_LIBWEBRTC_SRC
      (cd $MOZ_LIBWEBRTC_SRC && \\
       git am mailbox.patch)
"
# if we're not in the resume situation from fast-forward-libwebrtc.sh
if [ "x$RESUME" = "x" ]; then
  # start off by verifying the vendoring process to make sure no changes have
  # been added to elm to fix bugs.
  bash dom/media/webrtc/third_party_build/verify_vendoring.sh
fi
ERROR_HELP=""

for (( ; ; )); do

find_base_commit
find_next_commit

echo "============ loop ff ============" 2>&1| tee -a $LOOP_OUTPUT_LOG

ERROR_HELP=$"Some portion of the detection and/or fixing of upstream revert commits
has failed.  Please fix the state of the git hub repo at: $MOZ_LIBWEBRTC_SRC.
When fixed, please resume this script with the following command:
    SKIP_NEXT_REVERT_CHK=1 bash dom/media/webrtc/third_party_build/loop-ff.sh
"
if [ "x$SKIP_NEXT_REVERT_CHK" == "x0" ]; then
  echo "===loop-ff=== Check for upcoming revert commit" 2>&1| tee -a $LOOP_OUTPUT_LOG
  AUTO_FIX_REVERT_AS_NOOP=1 bash dom/media/webrtc/third_party_build/detect_upstream_revert.sh 2>&1| tee -a $LOOP_OUTPUT_LOG
fi
SKIP_NEXT_REVERT_CHK="0"
ERROR_HELP=""

echo "===loop-ff=== looking for ~/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg"
if [ -f ~/$MOZ_LIBWEBRTC_NEXT_BASE.no-op-cherry-pick-msg ]; then
  echo "===loop-ff=== detected special commit msg, setting HANDLE_NOOP_COMMIT=1"
  HANDLE_NOOP_COMMIT="1"
fi

echo "===loop-ff=== Moving from moz-libwebrtc commit $MOZ_LIBWEBRTC_BASE to $MOZ_LIBWEBRTC_NEXT_BASE" 2>&1| tee -a $LOOP_OUTPUT_LOG
bash dom/media/webrtc/third_party_build/fast-forward-libwebrtc.sh 2>&1| tee -a $LOOP_OUTPUT_LOG

MOZ_CHANGED=`hg diff -c tip --stat \
   | egrep -ve "README.moz-ff-commit|README.mozilla|files changed," \
   | wc -l | tr -d " " || true`
GIT_CHANGED=`cd $MOZ_LIBWEBRTC_SRC ; \
   git show --oneline --name-only $MOZ_LIBWEBRTC_NEXT_BASE \
   | csplit -f gitshow -sk - 2 ; cat gitshow01 \
   | egrep -ve "^CODE_OF_CONDUCT.md|^ENG_REVIEW_OWNERS|^PRESUBMIT.py|^README.chromium|^WATCHLISTS|^abseil-in-webrtc.md|^codereview.settings|^license_template.txt|^native-api.md|^presubmit_test.py|^presubmit_test_mocks.py|^pylintrc|^style-guide.md" \
   | wc -l | tr -d " " || true`
FILE_CNT_MISMATCH_MSG=$"
The number of files changed in the upstream commit ($GIT_CHANGED) does
not match the number of files changed in the local Mozilla repo
commit ($MOZ_CHANGED).  This may indicate a mismatch between the vendoring
script and this script, or it could be a true error in the import
processing.  Once the issue has been resolved, the following steps
remain for this commit:
  # generate moz.build files (may not be necessary)
  ./mach python python/mozbuild/mozbuild/gn_processor.py \\
      dom/media/webrtc/third_party_build/gn-configs/webrtc.json
  # commit the updated moz.build files with the appropriate commit msg
  bash dom/media/webrtc/third_party_build/commit-build-file-changes.sh
  # do a (hopefully) quick test build
  ./mach build
"
echo "===loop-ff=== Verify number of files changed MOZ($MOZ_CHANGED) GIT($GIT_CHANGED)" 2>&1| tee -a $LOOP_OUTPUT_LOG
if [ "x$HANDLE_NOOP_COMMIT" == "x1" ]; then
  echo "===loop-ff=== NO-OP commit detected, we expect file changed counts to differ" 2>&1| tee -a $LOOP_OUTPUT_LOG
elif [ $MOZ_CHANGED -ne $GIT_CHANGED ]; then
  echo "MOZ_CHANGED $MOZ_CHANGED should equal GIT_CHANGED $GIT_CHANGED" 2>&1| tee -a $LOOP_OUTPUT_LOG
  echo "$FILE_CNT_MISMATCH_MSG"
  exit 1
fi
HANDLE_NOOP_COMMIT=""

MODIFIED_BUILD_RELATED_FILE_CNT=`hg diff -c tip --stat \
    --include 'third_party/libwebrtc/**BUILD.gn' \
    --include 'third_party/libwebrtc/webrtc.gni' \
    | grep -v "files changed" \
    | wc -l | tr -d " " || true`
echo "===loop-ff=== Modified BUILD.gn (or webrtc.gni) files: $MODIFIED_BUILD_RELATED_FILE_CNT" 2>&1| tee -a $LOOP_OUTPUT_LOG
if [ "x$MODIFIED_BUILD_RELATED_FILE_CNT" != "x0" ]; then
  echo "===loop-ff=== Regenerate build files" 2>&1| tee -a $LOOP_OUTPUT_LOG
  ./mach python python/mozbuild/mozbuild/gn_processor.py \
      dom/media/webrtc/third_party_build/gn-configs/webrtc.json 2>&1| tee -a $LOOP_OUTPUT_LOG

  MOZ_BUILD_CHANGE_CNT=`hg status third_party/libwebrtc \
      --include 'third_party/libwebrtc/**moz.build' | wc -l | tr -d " "`
  if [ "x$MOZ_BUILD_CHANGE_CNT" != "x0" ]; then
    echo "===loop-ff=== Detected modified moz.build files, commiting" 2>&1| tee -a $LOOP_OUTPUT_LOG
  fi

  bash dom/media/webrtc/third_party_build/commit-build-file-changes.sh 2>&1| tee -a $LOOP_OUTPUT_LOG
fi

ERROR_HELP=$"
The test build has failed.  Most likely this is due to an upstream api change that
must be reflected in Mozilla code outside of the third_party/libwebrtc directory.
"
echo "===loop-ff=== Test build" 2>&1| tee -a $LOOP_OUTPUT_LOG
./mach build 2>&1| tee -a $LOOP_OUTPUT_LOG
ERROR_HELP=""

if [ ! "x$MOZ_STOP_AFTER_COMMIT" = "x" ]; then
if [ $MOZ_LIBWEBRTC_NEXT_BASE = $MOZ_STOP_AFTER_COMMIT ]; then
  break
fi
fi

done

echo "===loop-ff=== Completed fast-foward to $MOZ_STOP_AFTER_COMMIT" 2>&1| tee -a $LOOP_OUTPUT_LOG
