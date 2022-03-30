#!/bin/bash

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'echo "*** ERROR *** $? $LINENO fast-forward-libwebrtc did not complete successfully!"' ERR 

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

if [ "x$MOZ_LIBWEBRTC_COMMIT" = "x" ]; then
  echo "MOZ_LIBWEBRTC_COMMIT is not defined, see README.md"
  exit
fi


RESUME=""
if [ -f log_resume.txt ]; then
  RESUME=`tail -1 log_resume.txt`
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


# read the last line of README.moz-ff-commit to retrieve our current base
# commit in moz-libwebrtc
MOZ_LIBWEBRTC_BASE=`tail -1 third_party/libwebrtc/README.moz-ff-commit`
# calculate the next commit above our current base commit
MOZ_LIBWEBRTC_NEXT_BASE=`cd $MOZ_LIBWEBRTC_SRC ; \
git log --oneline --reverse --ancestry-path $MOZ_LIBWEBRTC_BASE^..main \
 | head -2 | tail -1 | awk '{print$1;}'`

UPSTREAM_ADDED_FILES=""

# After this point:
# * eE: All commands should succede.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succede.
set -eEuo pipefail

echo "     MOZ_LIBWEBRTC_BASE: $MOZ_LIBWEBRTC_BASE"
echo "MOZ_LIBWEBRTC_NEXT_BASE: $MOZ_LIBWEBRTC_NEXT_BASE"
echo " RESUME: $RESUME"
echo "SKIP_TO: $SKIP_TO"

echo "-------"
echo "------- Write cmd-line to third_party/libwebrtc/README.moz-ff-commit"
echo "-------"
echo "# MOZ_LIBWEBRTC_SRC=$MOZ_LIBWEBRTC_SRC MOZ_LIBWEBRTC_COMMIT=$MOZ_LIBWEBRTC_COMMIT bash $0" \
    >> third_party/libwebrtc/README.moz-ff-commit

echo "-------"
echo "------- Write new-base to last line of third_party/libwebrtc/README.moz-ff-commit"
echo "-------"
echo "# base of lastest vendoring" >> third_party/libwebrtc/README.moz-ff-commit
echo "$MOZ_LIBWEBRTC_NEXT_BASE" >> third_party/libwebrtc/README.moz-ff-commit


function rebase_mozlibwebrtc_stack {
  echo "-------"
  echo "------- Rebase $MOZ_LIBWEBRTC_COMMIT to $MOZ_LIBWEBRTC_NEXT_BASE"
  echo "-------"
  ( cd $MOZ_LIBWEBRTC_SRC && \
    git checkout -q $MOZ_LIBWEBRTC_COMMIT && \
    git rebase $MOZ_LIBWEBRTC_NEXT_BASE \
    &> log-rebase-moz-libwebrtc.txt \
  )
}

function vendor_off_next_commit {
  echo "-------"
  echo "------- Vendor $MOZ_LIBWEBRTC_COMMIT from $MOZ_LIBWEBRTC_SRC"
  echo "-------"
  ( cd dom/media/webrtc/third_party_build && \
    python3 vendor-libwebrtc.py \
            --from-local $MOZ_LIBWEBRTC_SRC \
            --commit $MOZ_LIBWEBRTC_COMMIT \
            libwebrtc \
  )
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
    third_party/libwebrtc &> log-regen-mozbuild-files.txt
}

function add_new_upstream_files {
  UPSTREAM_ADDED_FILES=`cd $MOZ_LIBWEBRTC_SRC && \
    git diff -r $MOZ_LIBWEBRTC_BASE -r $MOZ_LIBWEBRTC_NEXT_BASE \
        --name-status --diff-filter=A \
        | awk '{print $2;}'`
  if [ "x$UPSTREAM_ADDED_FILES" != "x" ]; then
    echo "-------"
    echo "------- Add new upstream files"
    echo "-------"
    (cd third_party/libwebrtc && hg add $UPSTREAM_ADDED_FILES)
    echo "$UPSTREAM_ADDED_FILES" &> log-new-upstream-files.txt
  fi
}

function remove_deleted_upstream_files {
  UPSTREAM_DELETED_FILES=`cd $MOZ_LIBWEBRTC_SRC && \
    git diff -r $MOZ_LIBWEBRTC_BASE -r $MOZ_LIBWEBRTC_NEXT_BASE \
        --name-status --diff-filter=D \
        | awk '{print $2;}'`
  if [ "x$UPSTREAM_DELETED_FILES" != "x" ]; then
    echo "-------"
    echo "------- Remove deleted upstream files"
    echo "-------"
    (cd third_party/libwebrtc && hg rm $UPSTREAM_DELETED_FILES)
    echo "$UPSTREAM_DELETED_FILES" &> log-deleted-upstream-files.txt
  fi
}

function handle_renamed_upstream_files {
  UPSTREAM_RENAMED_FILES=`cd $MOZ_LIBWEBRTC_SRC && \
    git diff -r $MOZ_LIBWEBRTC_BASE -r $MOZ_LIBWEBRTC_NEXT_BASE \
        --name-status --diff-filter=R \
        | awk '{print $2 " " $3;}'`
  if [ "x$UPSTREAM_RENAMED_FILES" != "x" ]; then
    echo "-------"
    echo "------- Handle renamed upstream files"
    echo "-------"
    (cd third_party/libwebrtc && echo "$UPSTREAM_RENAMED_FILES" | while read line; do hg rename --after $line; done)
    echo "$UPSTREAM_RENAMED_FILES" &> log-renamed-upstream-files.txt
  fi
}

if [ $SKIP_TO = "run" ]; then
  echo "resume2" > log_resume.txt
  rebase_mozlibwebrtc_stack;
fi

if [ $SKIP_TO = "resume2" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume3" > log_resume.txt
  vendor_off_next_commit;
fi

if [ $SKIP_TO = "resume3" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume4" > log_resume.txt
  regen_mozbuild_files;
fi

if [ $SKIP_TO = "resume4" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume5" > log_resume.txt
  remove_deleted_upstream_files;
fi

if [ $SKIP_TO = "resume5" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "resume6" > log_resume.txt
  add_new_upstream_files;
fi

if [ $SKIP_TO = "resume6" ]; then SKIP_TO="run"; fi
if [ $SKIP_TO = "run" ]; then
  echo "" > log_resume.txt
  handle_renamed_upstream_files;
fi

echo "-------"
echo "------- Commit vendored changes from $MOZ_LIBWEBRTC_NEXT_BASE"
echo "-------"
hg commit -m "Bug 1766646 - Vendor libwebrtc from $MOZ_LIBWEBRTC_NEXT_BASE"
