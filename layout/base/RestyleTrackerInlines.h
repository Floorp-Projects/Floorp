/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleTrackerInlines_h
#define mozilla_RestyleTrackerInlines_h

#ifdef RESTYLE_LOGGING
bool
mozilla::RestyleTracker::ShouldLogRestyle()
{
  return mRestyleManager->ShouldLogRestyle();
}

int32_t&
mozilla::RestyleTracker::LoggingDepth()
{
  return mRestyleManager->LoggingDepth();
}
#endif

#endif // !defined(mozilla_RestyleTrackerInlines_h)
