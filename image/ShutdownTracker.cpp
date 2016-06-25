/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShutdownTracker.h"

#include "mozilla/Services.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

namespace mozilla {
namespace image {

class ShutdownTrackerImpl;

///////////////////////////////////////////////////////////////////////////////
// Static Data
///////////////////////////////////////////////////////////////////////////////

// Whether we've observed shutdown starting yet.
static bool sShutdownHasStarted = false;


///////////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////////

struct ShutdownObserver : public nsIObserver
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports*, const char* aTopic, const char16_t*) override
  {
    if (strcmp(aTopic, "xpcom-will-shutdown") != 0) {
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "xpcom-will-shutdown");
    }

    sShutdownHasStarted = true;
    return NS_OK;
  }

private:
  virtual ~ShutdownObserver() { }
};

NS_IMPL_ISUPPORTS(ShutdownObserver, nsIObserver)


///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/* static */ void
ShutdownTracker::Initialize()
{
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(new ShutdownObserver, "xpcom-will-shutdown", false);
  }
}

/* static */ bool
ShutdownTracker::ShutdownHasStarted()
{
  return sShutdownHasStarted;
}

} // namespace image
} // namespace mozilla
