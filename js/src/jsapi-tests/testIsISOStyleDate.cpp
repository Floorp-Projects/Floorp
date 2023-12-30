/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Date.h"
#include "jsapi-tests/tests.h"

const char* VALID_DATES[] = {
    "2009",
    "2009-05",
    "2009-05-19",
    "2022-02-29",
    "2009T15:00",
    "2009-05T15:00",
    "2022-06-31T15:00",
    "2009-05-19T15:00",
    "2009-05-19T15:00:15",
    "2009-05-19T15:00-00:00",
    "2009-05-19T15:00:15.452",
    "2009-05-19T15:00:15.452Z",
    "2009-05-19T15:00:15.452+02:00",
    "2009-05-19T15:00:15.452-02:00",
    "-271821-04-20T00:00:00Z",
    "+000000-01-01T00:00:00Z",
};

const char* INVALID_DATES[] = {
    "10",
    "20009",
    "+20009",
    "2009-",
    "2009-0",
    "2009-15",
    "2009-02-1",
    "2009-02-50",
    "15:00",
    "T15:00",
    "9-05-19T15:00",
    "2009-5-19T15:00",
    "2009-05-1T15:00",
    "2009-02-10T15",
    "2009-05-19T15:",
    "2009-05-19T1:00",
    "2009-05-19T10:1",
    "2009-05-19T60:00",
    "2009-05-19T15:70",
    "2009-05-19T15:00.25",
    "2009-05-19+10:00",
    "2009-05-19Z",
    "2009-05-19 15:00",
    "2009-05-19t15:00Z",
    "2009-05-19T15:00z",
    "2009-05-19T15:00+01",
    "2009-05-19T10:10+1:00",
    "2009-05-19T10:10+01:1",
    "2009-05-19T15:00+75:00",
    "2009-05-19T15:00+02:80",
    "02009-05-19T15:00",
};

BEGIN_TEST(testIsISOStyleDate_success) {
  for (const char* date : VALID_DATES) {
    CHECK(ValidDate(date));
  }
  for (const char* date : INVALID_DATES) {
    CHECK(!ValidDate(date));
  }

  return true;
}

bool ValidDate(const char* str) {
  return JS::IsISOStyleDate(cx, JS::Latin1Chars(str, strlen(str)));
}

END_TEST(testIsISOStyleDate_success)
