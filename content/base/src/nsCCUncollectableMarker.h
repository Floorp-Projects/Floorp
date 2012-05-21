/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserver.h"
#include "nsCycleCollectionParticipant.h"

struct JSTracer;

class nsCCUncollectableMarker : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Inits a global nsCCUncollectableMarker. Should only be called once.
   */
  static nsresult Init();

  /**
   * Checks if we're collecting during a given generation
   */
  static bool InGeneration(PRUint32 aGeneration)
  {
    return aGeneration && aGeneration == sGeneration;
  }

  static bool InGeneration(nsCycleCollectionTraversalCallback& aCb,
                           PRUint32 aGeneration)
  {
    return InGeneration(aGeneration) && !aCb.WantAllTraces();
  }

  static PRUint32 sGeneration;

private:
  nsCCUncollectableMarker() {}

};

namespace mozilla {
namespace dom {
void TraceBlackJS(JSTracer* aTrc);
}
}
