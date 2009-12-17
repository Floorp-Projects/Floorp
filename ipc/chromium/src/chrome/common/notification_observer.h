// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOTIFICATION_OBSERVER_H_
#define CHROME_COMMON_NOTIFICATION_OBSERVER_H_

class NotificationDetails;
class NotificationSource;
class NotificationType;

// This is the base class for notification observers. When a matching
// notification is posted to the notification service, Observe is called.
class NotificationObserver {
 public:
  virtual ~NotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) = 0;
};

#endif  // CHROME_COMMON_NOTIFICATION_OBSERVER_H_
