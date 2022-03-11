#!/bin/sh
# This script download samples for testing

clean=1
cleanall=0
sample_dir=autofill-repo-samples

while [ $# -gt 0 ]; do
    case "$1" in
      -c) clean=1 ;;
      -cc) cleanall=1 ;;
    esac
    shift
done

# Check out source code
if ! [ -d "fathom-form-autofill" ]; then
  echo "Get samples from repo..."
  git clone https://github.com/mozilla-services/fathom-form-autofill
fi

if ! [ -d "samples" ]; then
  echo "Copy samples..."
  mkdir $sample_dir
  cp -r fathom-form-autofill/samples/testing $sample_dir
  cp -r fathom-form-autofill/samples/training $sample_dir
  cp -r fathom-form-autofill/samples/validation $sample_dir
else
  echo "\`samples\` directory already exists"
fi

if [ "$clean" = 1 ] || [ "$cleanall" = 1 ]; then
  echo "Cleanup..."
  rm -rf fathom-form-autofill
fi

if [ "$cleanall" = 1 ]; then
  rm -rf $sample_dir
fi
