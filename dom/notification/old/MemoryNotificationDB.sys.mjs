/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { NotificationDB } from "./NotificationDB.sys.mjs";

class MemoryNotificationDB extends NotificationDB {
  storageQualifier() {
    return "MemoryNotification";
  }

  async load() {
    this.loaded = true;
  }

  async createStore() {}
  async createFile() {}
  async save() {}
}

new MemoryNotificationDB();
