#!/bin/bash
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
#
# Script to generate <video src> reftest files, from corresponding reftest
# files that use <video poster>.
#
# This script expects to be run from this working directory:
#  mozilla-central/layout/reftests/w3c-css/submitted/images3
#
# It requires that the tools png2yuv and vpxenc be available, from the
# Ubuntu packages 'mjpegtools' and 'vpx-tools'. More info here:
#  http://wiki.webmproject.org/howtos/convert-png-frames-to-webm-video

VIDEO_REFTEST_PATH="../../../webm-video"

imageFileArr=("colors-16x8.png" "colors-8x16.png")
numImageFiles=${#imageFileArr[@]}

# Copy image files, and generate webm files:
for ((i = 0; i < $numImageFiles; i++)); do
  imageFileName=${imageFileArr[$i]}
  imageDest=$VIDEO_REFTEST_PATH/$imageFileName

  echo "Copying $imageDest."
  hg cp support/$imageFileName $imageDest

  videoDest=`echo $imageDest | sed "s/png/webm/"`
  echo "Generating $videoDest."
  png2yuv -f 1 -I p -b 1 -n 1 -j $imageDest \
    | vpxenc --passes=1 --pass=1 --codec=vp9 --lossless=1 --max-q=0 -o $videoDest -
  hg add $videoDest
done

reftestListFileName="$VIDEO_REFTEST_PATH/reftest.list"

# Loop across all <video poster> tests:
for origTestName in object-position-png-*p.html; do
  # Find the corresponding reference case:
  origReferenceName=$(echo $origTestName |
                      sed "s/p.html/-ref.html/")

  # The generated testcase will have "-webm" instead of "-png", and unlike
  # the original png test, it won't have a single-letter suffix to indicate the
  # particular container element. (since it's unnecessary -- there's only one
  # possible container element for webm, "<video>")
  videoTestName=$(echo $origTestName |
                  sed "s/png/webm/" |
                  sed "s/p.html/.html/")

  videoReferenceName=$(echo $videoTestName |
                       sed "s/.html/-ref.html/")

  # Generate reference file (dropping "support" subdir from image paths):
  echo "Copying $origReferenceName to $VIDEO_REFTEST_PATH."
  videoReferenceFullPath=$VIDEO_REFTEST_PATH/$videoReferenceName
  hg cp $origReferenceName $videoReferenceFullPath
  sed -i "s,support/,," $videoReferenceFullPath

  # Generate testcase
  # (converting <video poster="support/foo.png"> to <video src="foo.webm">):
  echo "Generating $videoTestName from $origTestName."
  videoTestFullPath=$VIDEO_REFTEST_PATH/$videoTestName
  hg cp $origTestName $videoTestFullPath
  sed -i "s/PNG image/WebM video/" $videoTestFullPath
  sed -i "s/poster/src/" $videoTestFullPath
  sed -i "s,support/,," $videoTestFullPath
  sed -i "s/png/webm/" $videoTestFullPath
  sed -i "s/$origReferenceName/$videoReferenceName/" $videoTestFullPath

  # Update reftest manifest:
  annotation="fails-if(layersGPUAccelerated) skip-if(Android||B2G)"
  comment="# Bug 1098417 for across-the-board failure, Bug 1083516 for layersGPUAccelerated failures, Bug 1084564 for Android/B2G failures"
  echo "$annotation == $videoTestName $videoReferenceName $comment" \
    >> $reftestListFileName

done
