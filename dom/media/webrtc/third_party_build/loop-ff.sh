#!/bin/bash

# file for logging loop script output
LOOP_OUTPUT_LOG=~/log-loop-ff.txt

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'echo "*** ERROR *** $? $LINENO loop-ff did not complete successfully!" |& tee -a $LOOP_OUTPUT_LOG' ERR 

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

MOZ_CHANGED=0
GIT_CHANGED=0

# After this point:
# * eE: All commands should succede.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succede.
set -eEuo pipefail

# start a new log with every run of this script
rm -f $LOOP_OUTPUT_LOG
# make sure third_party/libwebrtc/README.moz-ff-commit is the committed version
# so we properly determine MOZ_LIBWEBRTC_BASE and MOZ_LIBWEBRTC_NEXT_BASE
# in the loop below
hg revert -C third_party/libwebrtc/README.moz-ff-commit &> /dev/null

for (( ; ; )); do

# read the last line of README.moz-ff-commit to retrieve our current base
# commit in moz-libwebrtc
MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`
# calculate the next commit above our current base commit
MOZ_LIBWEBRTC_NEXT_BASE=`cd $MOZ_LIBWEBRTC_SRC ; \
git log --oneline --ancestry-path $MOZ_LIBWEBRTC_BASE^..main \
 | tail -2 | head -1 | awk '{print$1;}'`

echo "============ loop ff ============" |& tee -a $LOOP_OUTPUT_LOG
echo "===loop-ff=== Moving from moz-libwebrtc commit $MOZ_LIBWEBRTC_BASE to $MOZ_LIBWEBRTC_NEXT_BASE" |& tee -a $LOOP_OUTPUT_LOG
bash dom/media/webrtc/third_party_build/fast-forward-libwebrtc.sh |& tee -a $LOOP_OUTPUT_LOG
#bash dom/media/webrtc/third_party_build/fast-forward-libwebrtc.sh

MOZ_CHANGED=`hg diff -c tip --stat | egrep -ve "README.moz-ff-commit|README.mozilla|files changed," | wc -l` || echo -n "0" > /dev/null
GIT_CHANGED=`cd $MOZ_LIBWEBRTC_SRC ; git show --oneline --name-only $MOZ_LIBWEBRTC_NEXT_BASE | wc -l`
let GIT_CHANGED--
echo "===loop-ff=== Verify number of files changed MOZ($MOZ_CHANGED) GIT($GIT_CHANGED)" |& tee -a $LOOP_OUTPUT_LOG
if [ $MOZ_CHANGED -ne $GIT_CHANGED ]; then
  echo "MOZ_CHANGED $MOZ_CHANGED should equal GIT_CHANGED $GIT_CHANGED" |& tee -a $LOOP_OUTPUT_LOG
  exit 1
fi

MODIFIED_BUILD_RELATED_FILE_CNT=`hg diff -c tip --stat \
    --include 'third_party/libwebrtc/**BUILD.gn' \
    --include 'third_party/libwebrtc/webrtc.gni' \
    | wc -l`
echo "===loop-ff=== Modified BUILD.gn (or webrtc.gni) files: $MODIFIED_BUILD_RELATED_FILE_CNT" |& tee -a $LOOP_OUTPUT_LOG
if [ "x$MODIFIED_BUILD_RELATED_FILE_CNT" != "x0" ]; then
  echo "===loop-ff=== Regenerate build files" |& tee -a $LOOP_OUTPUT_LOG
  ./mach python python/mozbuild/mozbuild/gn_processor.py \
      dom/media/webrtc/third_party_build/gn-configs/webrtc.json |& tee -a $LOOP_OUTPUT_LOG

  MOZ_BUILD_CHANGE_CNT=`hg status third_party/libwebrtc \
      --include 'third_party/libwebrtc/**moz.build' | wc -l`
  if [ "x$MOZ_BUILD_CHANGE_CNT" != "x0" ]; then
    echo "===loop-ff=== Detected modified moz.build files, commiting" |& tee -a $LOOP_OUTPUT_LOG
  fi

  export UPSTREAM_COMMIT=$MOZ_LIBWEBRTC_NEXT_BASE
  bash dom/media/webrtc/third_party_build/commit-build-file-changes.sh |& tee -a $LOOP_OUTPUT_LOG
fi

echo "===loop-ff=== Test build" |& tee -a $LOOP_OUTPUT_LOG
./mach build |& tee -a $LOOP_OUTPUT_LOG

if [ ! "x$MOZ_STOP_AFTER_COMMIT" = "x" ]; then
if [ $MOZ_LIBWEBRTC_NEXT_BASE = $MOZ_STOP_AFTER_COMMIT ]; then
  break
fi
fi

done

echo "===loop-ff=== Completed fast-foward to $MOZ_STOP_AFTER_COMMIT" |& tee -a $LOOP_OUTPUT_LOG
