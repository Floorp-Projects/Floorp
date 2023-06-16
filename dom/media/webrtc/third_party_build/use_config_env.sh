#!/bin/bash

# Assume that if STATE_DIR is already defined, we do not need to
# execute this file again.
if [ "x$STATE_DIR" != "x" ]; then
  # no need to run script since we've already run
  return
fi

export SCRIPT_DIR="dom/media/webrtc/third_party_build"
# first, make sure we're running from the top of moz-central repo
if [ ! -d $SCRIPT_DIR ]; then
  echo "Error: unable to find directory $SCRIPT_DIR"
  exit 1
fi

# Should we tie the location of the STATE_DIR to the path
# in MOZ_CONFIG_PATH?  Probably.
export STATE_DIR=`pwd`/.moz-fast-forward
export LOG_DIR=$STATE_DIR/logs
export TMP_DIR=$STATE_DIR/tmp

if [ ! -d $STATE_DIR ]; then
  echo "Creating missing $STATE_DIR"
  mkdir -p $STATE_DIR
  if [ ! -d $STATE_DIR ]; then
    echo "error: unable to find (or create) $STATE_DIR"
    exit 1
  fi
fi
echo "Using STATE_DIR=$STATE_DIR"

if [ ! -d $LOG_DIR ]; then
  echo "Creating missing $LOG_DIR"
  mkdir -p $LOG_DIR
  if [ ! -d $LOG_DIR ]; then
    echo "error: unable to find (or create) $LOG_DIR"
    exit 1
  fi
fi
echo "Using LOG_DIR=$LOG_DIR"

if [ ! -d $TMP_DIR ]; then
  echo "Creating missing $TMP_DIR"
  mkdir -p $TMP_DIR
  if [ ! -d $TMP_DIR ]; then
    echo "error: unable to find (or create) $TMP_DIR"
    exit 1
  fi
fi
echo "Using TMP_DIR=$TMP_DIR"

# Allow user to override default path to config_env
if [ "x$MOZ_CONFIG_PATH" = "x" ]; then
  MOZ_CONFIG_PATH=$STATE_DIR/config_env
  echo "Using default MOZ_CONFIG_PATH=$MOZ_CONFIG_PATH"
fi

if [ ! -f $MOZ_CONFIG_PATH ]; then
  echo "Creating default config file at $MOZ_CONFIG_PATH"
  cp $SCRIPT_DIR/default_config_env $MOZ_CONFIG_PATH
fi
source $MOZ_CONFIG_PATH


function find_base_commit()
{
  # read the last line of README.moz-ff-commit to retrieve our current base
  # commit in moz-libwebrtc
  MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`
  echo "prelim MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
  # if we've advanced into a chrome release branch, we need to adjust the
  # MOZ_LIBWEBRTC_BASE to the last common commit so we can now advance up
  # the trunk commits.
  MOZ_LIBWEBRTC_BASE=`cd $MOZ_LIBWEBRTC_SRC ; git merge-base $MOZ_LIBWEBRTC_BASE $MOZ_TARGET_UPSTREAM_BRANCH_HEAD`
  # now make it a short hash
  MOZ_LIBWEBRTC_BASE=`cd $MOZ_LIBWEBRTC_SRC ; git rev-parse --short $MOZ_LIBWEBRTC_BASE`
  echo "adjusted MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
}
export -f find_base_commit

function find_next_commit()
{
  # identify the next commit above our current base commit
  MOZ_LIBWEBRTC_NEXT_BASE=`cd $MOZ_LIBWEBRTC_SRC ; \
  git log --oneline --ancestry-path $MOZ_LIBWEBRTC_BASE^..$MOZ_TARGET_UPSTREAM_BRANCH_HEAD \
   | tail -2 | head -1 | awk '{print $1;}'`
}
export -f find_next_commit
