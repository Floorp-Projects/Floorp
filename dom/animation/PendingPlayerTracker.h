/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PendingPlayerTracker_h
#define mozilla_dom_PendingPlayerTracker_h

#include "mozilla/dom/AnimationPlayer.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsTHashtable.h"

namespace mozilla {

class PendingPlayerTracker MOZ_FINAL
{
public:
  explicit PendingPlayerTracker(nsIDocument* aDocument)
    : mDocument(aDocument)
  { }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PendingPlayerTracker)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(PendingPlayerTracker)

  void AddPlayPending(dom::AnimationPlayer& aPlayer);
  void RemovePlayPending(dom::AnimationPlayer& aPlayer);
  bool IsWaitingToPlay(dom::AnimationPlayer const& aPlayer) const;

  // Causes any pending players to resume at |aReadyTime| by first
  // fast-forwarding their timeline to the corresponding time.
  void StartPendingPlayers(const TimeStamp& aReadyTime);

private:
  ~PendingPlayerTracker() { }

  typedef nsTHashtable<nsRefPtrHashKey<dom::AnimationPlayer>>
    AnimationPlayerSet;

  AnimationPlayerSet mPlayPendingSet;
  nsCOMPtr<nsIDocument> mDocument;
};

} // namespace mozilla

#endif // mozilla_dom_PendingPlayerTracker_h
