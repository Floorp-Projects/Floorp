/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file describes a central switchboard for notifications that might
// happen in various parts of the application, and allows users to register
// observers for various classes of events that they're interested in.

#ifndef CHROME_COMMON_NOTIFICATION_SERVICE_H_
#define CHROME_COMMON_NOTIFICATION_SERVICE_H_

#include <map>

#include "base/observer_list.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

class NotificationObserver;

class NotificationService {
 public:
  // Returns the NotificationService object for the current thread, or NULL if
  // none.
  static NotificationService* current();

  // Normally instantiated when the thread is created.  Not all threads have
  // a NotificationService.  Only one instance should be created per thread.
  NotificationService();
  ~NotificationService();

  // Registers a NotificationObserver to be called whenever a matching
  // notification is posted.  Observer is a pointer to an object subclassing
  // NotificationObserver to be notified when an event matching the other two
  // parameters is posted to this service.  Type is the type of events to
  // be notified about (or NOTIFY_ALL to receive events of all types).
  // Source is a NotificationSource object (created using
  // "Source<classname>(pointer)"), if this observer only wants to
  // receive events from that object, or NotificationService::AllSources()
  // to receive events from all sources.
  //
  // A given observer can be registered only once for each combination of
  // type and source.  If the same object is registered more than once,
  // it must be removed for each of those combinations of type and source later.
  //
  // The caller retains ownership of the object pointed to by observer.
  void AddObserver(NotificationObserver* observer,
                   NotificationType type, const NotificationSource& source);

  // Removes the object pointed to by observer from receiving notifications
  // that match type and source.  If no object matching the parameters is
  // currently registered, this method is a no-op.
  void RemoveObserver(NotificationObserver* observer,
                      NotificationType type, const NotificationSource& source);

  // Synchronously posts a notification to all interested observers.
  // Source is a reference to a NotificationSource object representing
  // the object originating the notification (can be
  // NotificationService::AllSources(), in which case
  // only observers interested in all sources will be notified).
  // Details is a reference to an object containing additional data about
  // the notification.  If no additional data is needed, NoDetails() is used.
  // There is no particular order in which the observers will be notified.
  void Notify(NotificationType type,
              const NotificationSource& source,
              const NotificationDetails& details);

  // Returns a NotificationSource that represents all notification sources
  // (for the purpose of registering an observer for events from all sources).
  static Source<void> AllSources() { return Source<void>(NULL); }

  // Returns a NotificationDetails object that represents a lack of details
  // associated with a notification.  (This is effectively a null pointer.)
  static Details<void> NoDetails() { return Details<void>(NULL); }

 private:
  typedef base::ObserverList<NotificationObserver> NotificationObserverList;
  typedef std::map<uintptr_t, NotificationObserverList*> NotificationSourceMap;

  // Convenience function to determine whether a source has a
  // NotificationObserverList in the given map;
  static bool HasKey(const NotificationSourceMap& map,
                     const NotificationSource& source);

  // Keeps track of the observers for each type of notification.
  // Until we get a prohibitively large number of notification types,
  // a simple array is probably the fastest way to dispatch.
  NotificationSourceMap observers_[NotificationType::NOTIFICATION_TYPE_COUNT];

#ifndef NDEBUG
  // Used to check to see that AddObserver and RemoveObserver calls are
  // balanced.
  int observer_counts_[NotificationType::NOTIFICATION_TYPE_COUNT];
#endif

  DISALLOW_COPY_AND_ASSIGN(NotificationService);
};

#endif  // CHROME_COMMON_NOTIFICATION_SERVICE_H_
