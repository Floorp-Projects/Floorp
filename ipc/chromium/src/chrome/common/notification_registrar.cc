// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notification_registrar.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/common/notification_service.h"

struct NotificationRegistrar::Record {
  bool operator==(const Record& other) const;

  NotificationObserver* observer;
  NotificationType type;
  NotificationSource source;
};

bool NotificationRegistrar::Record::operator==(const Record& other) const {
  return observer == other.observer &&
         type == other.type &&
         source == other.source;
}

NotificationRegistrar::NotificationRegistrar() {
}

NotificationRegistrar::~NotificationRegistrar() {
  RemoveAll();
}

void NotificationRegistrar::Add(NotificationObserver* observer,
                                NotificationType type,
                                const NotificationSource& source) {
  Record record = { observer, type, source };
  DCHECK(std::find(registered_.begin(), registered_.end(), record) ==
         registered_.end()) << "Duplicate registration.";
  registered_.push_back(record);

  NotificationService::current()->AddObserver(observer, type, source);
}

void NotificationRegistrar::Remove(NotificationObserver* observer,
                                   NotificationType type,
                                   const NotificationSource& source) {
  Record record = { observer, type, source };
  RecordVector::iterator found = std::find(
      registered_.begin(), registered_.end(), record);
  if (found != registered_.end()) {
    registered_.erase(found);
  } else {
    // Fall through to passing the removal through to the notification service.
    // If it really isn't registered, it will also assert and do nothing, but
    // we might as well catch the case where the class is trying to unregister
    // for something they registered without going through us.
    NOTREACHED();
  }

  NotificationService::current()->RemoveObserver(observer, type, source);
}

void NotificationRegistrar::RemoveAll() {
  NotificationService* service = NotificationService::current();
  for (size_t i = 0; i < registered_.size(); i++) {
    service->RemoveObserver(registered_[i].observer,
                            registered_[i].type,
                            registered_[i].source);
  }
  registered_.clear();
}
