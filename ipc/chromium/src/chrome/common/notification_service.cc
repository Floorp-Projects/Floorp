// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notification_service.h"

#include "base/lazy_instance.h"
#include "base/thread_local.h"

static base::LazyInstance<base::ThreadLocalPointer<NotificationService> >
    lazy_tls_ptr(base::LINKER_INITIALIZED);

// static
NotificationService* NotificationService::current() {
  return lazy_tls_ptr.Pointer()->Get();
}

// static
bool NotificationService::HasKey(const NotificationSourceMap& map,
                                 const NotificationSource& source) {
  return map.find(source.map_key()) != map.end();
}

NotificationService::NotificationService() {
  DCHECK(current() == NULL);
#ifndef NDEBUG
  memset(observer_counts_, 0, sizeof(observer_counts_));
#endif

  lazy_tls_ptr.Pointer()->Set(this);
}

void NotificationService::AddObserver(NotificationObserver* observer,
                                      NotificationType type,
                                      const NotificationSource& source) {
  DCHECK(type.value < NotificationType::NOTIFICATION_TYPE_COUNT);

  // We have gotten some crashes where the observer pointer is NULL. The problem
  // is that this happens when we actually execute a notification, so have no
  // way of knowing who the bad observer was. We want to know when this happens
  // in release mode so we know what code to blame the crash on (since this is
  // guaranteed to crash later).
  CHECK(observer);

  NotificationObserverList* observer_list;
  if (HasKey(observers_[type.value], source)) {
    observer_list = observers_[type.value][source.map_key()];
  } else {
    observer_list = new NotificationObserverList;
    observers_[type.value][source.map_key()] = observer_list;
  }

  observer_list->AddObserver(observer);
#ifndef NDEBUG
  ++observer_counts_[type.value];
#endif
}

void NotificationService::RemoveObserver(NotificationObserver* observer,
                                         NotificationType type,
                                         const NotificationSource& source) {
  DCHECK(type.value < NotificationType::NOTIFICATION_TYPE_COUNT);
  DCHECK(HasKey(observers_[type.value], source));

  NotificationObserverList* observer_list =
      observers_[type.value][source.map_key()];
  if (observer_list) {
    observer_list->RemoveObserver(observer);
#ifndef NDEBUG
    --observer_counts_[type.value];
#endif
  }

  // TODO(jhughes): Remove observer list from map if empty?
}

void NotificationService::Notify(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(type.value > NotificationType::ALL) <<
      "Allowed for observing, but not posting.";
  DCHECK(type.value < NotificationType::NOTIFICATION_TYPE_COUNT);

  // There's no particular reason for the order in which the different
  // classes of observers get notified here.

  // Notify observers of all types and all sources
  if (HasKey(observers_[NotificationType::ALL], AllSources()) &&
      source != AllSources()) {
    FOR_EACH_OBSERVER(NotificationObserver,
       *observers_[NotificationType::ALL][AllSources().map_key()],
       Observe(type, source, details));
  }

  // Notify observers of all types and the given source
  if (HasKey(observers_[NotificationType::ALL], source)) {
    FOR_EACH_OBSERVER(NotificationObserver,
        *observers_[NotificationType::ALL][source.map_key()],
        Observe(type, source, details));
  }

  // Notify observers of the given type and all sources
  if (HasKey(observers_[type.value], AllSources()) &&
      source != AllSources()) {
    FOR_EACH_OBSERVER(NotificationObserver,
                      *observers_[type.value][AllSources().map_key()],
                      Observe(type, source, details));
  }

  // Notify observers of the given type and the given source
  if (HasKey(observers_[type.value], source)) {
    FOR_EACH_OBSERVER(NotificationObserver,
                      *observers_[type.value][source.map_key()],
                      Observe(type, source, details));
  }
}


NotificationService::~NotificationService() {
  lazy_tls_ptr.Pointer()->Set(NULL);

#ifndef NDEBUG
  for (int i = 0; i < NotificationType::NOTIFICATION_TYPE_COUNT; i++) {
    if (observer_counts_[i] > 0) {
      // This may not be completely fixable -- see
      // http://code.google.com/p/chromium/issues/detail?id=11010 .
      // But any new leaks should be fixed.
      LOG(WARNING) << observer_counts_[i] << " notification observer(s) leaked"
          << " of notification type " << i;
    }
  }
#endif

  for (int i = 0; i < NotificationType::NOTIFICATION_TYPE_COUNT; i++) {
    NotificationSourceMap omap = observers_[i];
    for (NotificationSourceMap::iterator it = omap.begin();
         it != omap.end(); ++it) {
      delete it->second;
    }
  }
}

NotificationObserver::~NotificationObserver() {}
