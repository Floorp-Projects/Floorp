/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum DeviceStorageAreaChangedEventOperation {
  "added",
  "removed",
  "unknown"
};

/*
 * For the EventHandler - onstorageareachanged, new event is introduced and called
 * DeviceStorageAreaChangedEvent.
 * The attribute - operation indicates that there is a storage area has been
 * added or removed.
 * 1. If a storage area is added, then the 'storageName' attribute will contain the
 * name of the storage area. To access this new storage area, a user needs to pass
 * storageName to navigator.getDeviceStorageByNameAndType to get a DeviceStorage object.
 * 2. If a storage area is removed, then the 'storageName' attribute indicates
 * which storage area was removed.
 */
[Pref="device.storage.enabled",
 Constructor(DOMString type, optional DeviceStorageAreaChangedEventInit eventInitDict)]
interface DeviceStorageAreaChangedEvent : Event {
  readonly attribute DeviceStorageAreaChangedEventOperation operation;
  readonly attribute DOMString                              storageName;
};

dictionary DeviceStorageAreaChangedEventInit : EventInit {
  DeviceStorageAreaChangedEventOperation operation = "unknown";
  DOMString                              storageName = "";
};
