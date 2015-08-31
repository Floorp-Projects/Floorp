/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef windowsmigrationutils__h__
#define windowsmigrationutils__h__

#include "prtime.h"

static
PRTime WinMigrationFileTimeToPRTime(FILETIME* filetime, bool* isValid)
{
  SYSTEMTIME st;
  *isValid = ::FileTimeToSystemTime(filetime, &st);
  if (!*isValid) {
    return 0;
  }
  PRExplodedTime prt;
  prt.tm_year = st.wYear;
  // SYSTEMTIME's day-of-month parameter is 1-based,
  // PRExplodedTime's is 0-based.
  prt.tm_month = st.wMonth - 1;
  prt.tm_mday = st.wDay;
  prt.tm_hour = st.wHour;
  prt.tm_min = st.wMinute;
  prt.tm_sec = st.wSecond;
  prt.tm_usec = st.wMilliseconds * 1000;
  prt.tm_wday = 0;
  prt.tm_yday = 0;
  prt.tm_params.tp_gmt_offset = 0;
  prt.tm_params.tp_dst_offset = 0;
  return PR_ImplodeTime(&prt);
}

#endif

