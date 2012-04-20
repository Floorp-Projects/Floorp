/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for the functions located directly in the "DownloadsCommon" object.
 */

function testFormatTimeLeft(aSeconds, aExpectedValue, aExpectedUnitString)
{
  let expected = "";
  if (aExpectedValue) {
    // Format the expected result based on the current language.
    expected = DownloadsCommon.strings[aExpectedUnitString](aExpectedValue);
  }
  do_check_eq(DownloadsCommon.formatTimeLeft(aSeconds), expected);
}

function run_test()
{
  testFormatTimeLeft(      0,   "", "");
  testFormatTimeLeft(      1,  "1", "shortTimeLeftSeconds");
  testFormatTimeLeft(     29, "29", "shortTimeLeftSeconds");
  testFormatTimeLeft(     30, "30", "shortTimeLeftSeconds");
  testFormatTimeLeft(     31,  "1", "shortTimeLeftMinutes");
  testFormatTimeLeft(     60,  "1", "shortTimeLeftMinutes");
  testFormatTimeLeft(     89,  "1", "shortTimeLeftMinutes");
  testFormatTimeLeft(     90,  "2", "shortTimeLeftMinutes");
  testFormatTimeLeft(     91,  "2", "shortTimeLeftMinutes");
  testFormatTimeLeft(   3600,  "1", "shortTimeLeftHours");
  testFormatTimeLeft(  86400, "24", "shortTimeLeftHours");
  testFormatTimeLeft( 169200, "47", "shortTimeLeftHours");
  testFormatTimeLeft( 172800,  "2", "shortTimeLeftDays");
  testFormatTimeLeft(8553600, "99", "shortTimeLeftDays");
  testFormatTimeLeft(8640000, "99", "shortTimeLeftDays");
}
