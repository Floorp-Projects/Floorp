/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimelineConsumers_h_
#define mozilla_TimelineConsumers_h_

#include "nsIObserver.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StaticMutex.h"
#include "nsTArray.h"
#include "TimelineMarkerEnums.h"  // for MarkerTracingType

class nsDocShell;
class nsIDocShell;
struct JSContext;

namespace mozilla {
class TimeStamp;
class MarkersStorage;
class AbstractTimelineMarker;

namespace dom {
struct ProfileTimelineMarker;
}

class TimelineConsumers : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

 private:
  TimelineConsumers();
  TimelineConsumers(const TimelineConsumers& aOther) = delete;
  void operator=(const TimelineConsumers& aOther) = delete;
  virtual ~TimelineConsumers() = default;

  bool Init();
  bool RemoveObservers();

 public:
  static already_AddRefed<TimelineConsumers> Get();

  // Methods for registering interested consumers (i.e. "devtools toolboxes").
  // Each consumer should be directly focused on a particular docshell, but
  // timeline markers don't necessarily have to be tied to that docshell.
  // See the public `AddMarker*` methods below.
  // Main thread only.
  void AddConsumer(nsDocShell* aDocShell);
  void RemoveConsumer(nsDocShell* aDocShell);

  bool HasConsumer(nsIDocShell* aDocShell);

  // Checks if there's any existing interested consumer.
  // May be called from any thread.
  bool IsEmpty();

  // Methods for adding markers relevant for particular docshells, or generic
  // (meaning that they either can't be tied to a particular docshell, or one
  // wasn't accessible in the part of the codebase where they're instantiated).
  // These will only add markers if at least one docshell is currently being
  // observed by a timeline. Markers tied to a particular docshell won't be
  // created unless that docshell is specifically being currently observed.
  // See nsIDocShell::recordProfileTimelineMarkers

  // These methods create a basic TimelineMarker from a name and some metadata,
  // relevant for a specific docshell.
  // Main thread only.
  void AddMarkerForDocShell(
      nsDocShell* aDocShell, const char* aName, MarkerTracingType aTracingType,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);
  void AddMarkerForDocShell(
      nsIDocShell* aDocShell, const char* aName, MarkerTracingType aTracingType,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);

  void AddMarkerForDocShell(
      nsDocShell* aDocShell, const char* aName, const TimeStamp& aTime,
      MarkerTracingType aTracingType,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);
  void AddMarkerForDocShell(
      nsIDocShell* aDocShell, const char* aName, const TimeStamp& aTime,
      MarkerTracingType aTracingType,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);

  // These methods register and receive ownership of an already created marker,
  // relevant for a specific docshell.
  // Main thread only.
  void AddMarkerForDocShell(nsDocShell* aDocShell,
                            UniquePtr<AbstractTimelineMarker>&& aMarker);
  void AddMarkerForDocShell(nsIDocShell* aDocShell,
                            UniquePtr<AbstractTimelineMarker>&& aMarker);

  // These methods create a basic marker from a name and some metadata,
  // which doesn't have to be relevant to a specific docshell.
  // May be called from any thread.
  void AddMarkerForAllObservedDocShells(
      const char* aName, MarkerTracingType aTracingType,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);
  void AddMarkerForAllObservedDocShells(
      const char* aName, const TimeStamp& aTime, MarkerTracingType aTracingType,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);

  // This method clones and registers an already instantiated marker,
  // which doesn't have to be relevant to a specific docshell.
  // May be called from any thread.
  void AddMarkerForAllObservedDocShells(
      UniquePtr<AbstractTimelineMarker>& aMarker);

  void PopMarkers(nsDocShell* aDocShell, JSContext* aCx,
                  nsTArray<dom::ProfileTimelineMarker>& aStore);

 private:
  static StaticRefPtr<TimelineConsumers> sInstance;
  static bool sInShutdown;

  // Counter for how many timelines are currently interested in markers,
  // and a list of the MarkersStorage interfaces representing them.
  unsigned long mActiveConsumers;
  LinkedList<MarkersStorage> mMarkersStores;

  // Protects this class's data structures.
  static StaticMutex sMutex;
};

}  // namespace mozilla

#endif /* mozilla_TimelineConsumers_h_ */
