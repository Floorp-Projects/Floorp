#!/bin/bash
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
#
# Script to generate XUL <image> reftest files, from corresponding reftest
# files that use <img>.
#
# This script expects to be run from this working directory:
#  mozilla-central/layout/reftests/w3c-css/submitted/images3

XUL_REFTEST_PATH="../../../xul"

imageFileArr=("colors-16x8.png"            "colors-8x16.png"
              "colors-16x8.svg"            "colors-8x16.svg"
              "colors-16x8-noSize.svg"     "colors-8x16-noSize.svg"
              "colors-16x8-parDefault.svg" "colors-8x16-parDefault.svg")
numImageFiles=${#imageFileArr[@]}

# Copy image files
for ((i = 0; i < $numImageFiles; i++)); do
  imageFileName=${imageFileArr[$i]}
  imageDest=$XUL_REFTEST_PATH/$imageFileName

  echo "Copying $imageDest."
  hg cp support/$imageFileName $imageDest
done

# Add comment & default-preferences line to reftest.list in dest directory:
reftestListFileName="$XUL_REFTEST_PATH/reftest.list"
echo "
# Tests for XUL <image> with 'object-fit' & 'object-position':
# These tests should be very similar to tests in our w3c-css/submitted/images3
# reftest directory. They live here because they use XUL, and it
# wouldn't be fair of us to make a W3C testsuite implicitly depend on XUL.
default-preferences test-pref(layout.css.object-fit-and-position.enabled,true)"\
  >> $reftestListFileName

# Loop across all object-fit tests that use <img> ("i" suffix):
for origTestName in object-fit*i.html; do
  newTestName=$(echo $origTestName |
                sed "s/i.html/.xul/")

  # Find the corresponding reference case:
  referenceName=$(echo $origTestName |
                  sed "s/i.html/-ref.html/")

  # Generate reference file (dropping "support" subdir from image paths):
  echo "Copying $referenceName to $XUL_REFTEST_PATH."
  newReferenceFullPath=$XUL_REFTEST_PATH/$referenceName
  hg cp $referenceName $newReferenceFullPath
  sed -i "s,support/,," $newReferenceFullPath

  # Generate testcase
  # (converting <video poster="support/foo.png"> to <video src="foo.webm">):
  echo "Generating $newTestName from $origTestName."
  newTestFullPath=$XUL_REFTEST_PATH/$newTestName
  hg cp $origTestName $newTestFullPath

  # Replace doctype with XML decl:
  sed -i "s/<!DOCTYPE html>/<?xml version=\"1.0\"?>/" $newTestFullPath

  # Replace html tags with window tags:
  sed -i "s,<html>,<window xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\">," $newTestFullPath
  sed -i "s,</html>,</window>," $newTestFullPath

  # Delete a bunch of HTML (not XUL) / W3C-testsuite boilerplate:
  sed -i "/head>/d" $newTestFullPath # Delete head open & close tags
  sed -i "/body>/d" $newTestFullPath # Delete body open & close tags
  sed -i "/<meta/d" $newTestFullPath # Delete meta charset tag
  sed -i "/<title/d" $newTestFullPath # Delete title line
  sed -i "/<link/d" $newTestFullPath # Delete link tags

  # Add 4px to all sizes, since in XUL, sizes are for border-box
  # instead of content-box.
  sed -i "s/ 48px/ 52px/" $newTestFullPath
  sed -i "s/ 32px/ 36px/" $newTestFullPath
  sed -i "s/ 8px/ 12px/" $newTestFullPath

  # Fix style open/close tags, and add 8px of padding on outer <window> to
  # match our HTML reference case, and change style rule to target <image>:
  sed -i "s,  <style type=\"text/css\">,\<style xmlns=\"http://www.w3.org/1999/xhtml\"><![CDATA[\n      window { padding: 8px; }," $newTestFullPath
  sed -i "s,  </style>,]]></style>," $newTestFullPath
  sed -i "s/img {/image {/" $newTestFullPath

  sed -i "s,support/,," $newTestFullPath
  sed -i "s,<img\(.*\)>,<image\1/>," $newTestFullPath
  sed -i "s,  <!--,<hbox>\n    <!--," $newTestFullPath
  sed -i "s,  <br>,</hbox>," $newTestFullPath

  # Update reftest manifest:
  echo "== $newTestName $referenceName" \
    >> $reftestListFileName

done
