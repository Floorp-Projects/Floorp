/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global FinalizationRegistry, WeakRef */

/**
 * This object implements iterable weak map for HTTP channels tracked by
 * the network observer.
 *
 * We can't use Map() for storing HTTP channel references since we don't
 * know when we should remove the entry in it (it's wrong to do it in
 * 'onTransactionClose' since it doesn't have to be the last platform
 * notification for a given channel). We want the map to auto update
 * when the channel is garbage collected.
 *
 * We can't use WeakMap() since searching for a value by the channel object
 * isn't reliable (there might be different objects representing the same
 * channel). We need to search by channel ID, but ID can't be used as key
 * in WeakMap().
 *
 * So, this custom map solves aforementioned issues.
 */
class ChannelMap {
  constructor() {
    this.weakMap = new WeakMap();
    this.refMap = new Map();
    this.finalizationGroup = new FinalizationRegistry(ChannelMap.cleanup);
  }

  static cleanup({ refMap, id }) {
    refMap.delete(id);
  }

  set(channel, value) {
    const ref = new WeakRef(channel);
    this.weakMap.set(channel, { value, ref });
    this.refMap.set(channel.channelId, ref);
    this.finalizationGroup.register(
      channel,
      {
        refMap: this.refMap,
        id: channel.channelId,
      },
      ref
    );
  }

  getChannelById(channelId) {
    const ref = this.refMap.get(channelId);
    const key = ref ? ref.deref() : null;
    if (!key) {
      return null;
    }
    const channelInfo = this.weakMap.get(key);
    return channelInfo ? channelInfo.value : null;
  }

  delete(channel) {
    const entry = this.weakMap.get(channel);
    if (!entry) {
      return false;
    }

    this.weakMap.delete(channel);
    this.refMap.delete(channel.channelId);
    this.finalizationGroup.unregister(entry.ref);
    return true;
  }

  clear() {
    this.refMap.clear();
  }
}

exports.ChannelMap = ChannelMap;
