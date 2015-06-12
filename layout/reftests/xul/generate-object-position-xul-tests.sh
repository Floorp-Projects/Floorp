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
# and it expects to be run *after* generate-object-fit-xul-tests.sh, since the
# copied test files will make use of image resources which are copied by that
# other script.

XUL_REFTEST_PATH="../../../xul"

reftestListFileName="$XUL_REFTEST_PATH/reftest.list"

# Loop across all object-position tests that use <img> ("i" suffix):
for origTestName in object-position-png-*i.html; do
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

  # Delete a bunch of HTML (not XUL) / W3C-testsuite boilerplate:
  sed -i "/head>/d" $newTestFullPath # Delete head open & close tags
  sed -i "/body>/d" $newTestFullPath # Delete body open & close tags
  sed -i "/<meta/d" $newTestFullPath # Delete meta charset tag
  sed -i "/<title/d" $newTestFullPath # Delete title line
  sed -i "/<link/d" $newTestFullPath # Delete link tags

  # Fix style open/close tags, and add 8px of padding on outer <window> to
  # match our HTML reference case, and change style rule to target <image>.
  # Also, add <hbox> to wrap the image elements.
  sed -i "s,  <style type=\"text/css\">,\<style xmlns=\"http://www.w3.org/1999/xhtml\"><![CDATA[\n      window { padding: 8px; }," $newTestFullPath
  sed -i "s,  </style>,]]></style>\n  <hbox>," $newTestFullPath
  sed -i "s,</html>,  </hbox>\n</window>," $newTestFullPath
  sed -i "s/img {/image {/" $newTestFullPath

  sed -i "s,support/,," $newTestFullPath
  sed -i "s,<img\(.*\)>,<image\1/>," $newTestFullPath

  # Update reftest manifest:
  echo "== $newTestName $referenceName" \
    >> $reftestListFileName

done
