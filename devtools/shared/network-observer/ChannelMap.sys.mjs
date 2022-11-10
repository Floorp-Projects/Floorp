/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * FinalizationRegistry callback, see
 * https://searchfox.org/mozilla-central/source/js/src/builtin/FinalizationRegistryObject.h
 *
 * Will be invoked when the channel corresponding to the weak reference is
 * "destroyed", at which point we can cleanup the corresponding entry in our
 * regular map.
 */
function deleteIdFromRefMap({ refMap, id }) {
  refMap.delete(id);
}

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
export class ChannelMap {
  #finalizationRegistry;
  #refMap;
  #weakMap;

  constructor() {
    // See https://searchfox.org/mozilla-central/source/js/src/builtin/FinalizationRegistryObject.h
    this.#finalizationRegistry = new FinalizationRegistry(deleteIdFromRefMap);

    // Map of channel id to a channel weak reference.
    this.#refMap = new Map();

    /**
     * WeakMap from nsIChannel instances to objects which encapsulate ChannelMap
     * values with the following structure:
     * @property {Object} value
     *     The actual value stored in this ChannelMap entry, which should relate
     *     to this channel.
     * @property {WeakRef} ref
     *     Weak reference for the channel object which is the key of the entry.
     */
    this.#weakMap = new WeakMap();
  }

  /**
   * Remove all entries from the ChannelMap.
   */
  clear() {
    this.#refMap.clear();
  }

  /**
   * Delete the entry for the provided channel from the underlying maps, if any.
   * Note that this will only delete entries which were set for the exact same
   * nsIChannel object, and will not attempt to look up entries by channel id.
   *
   * @param {nsIChannel} channel
   *     The key to delete from the ChannelMap.
   *
   * @return {boolean}
   *     True if an entry was deleted, false otherwise.
   */
  delete(channel) {
    const entry = this.#weakMap.get(channel);
    if (!entry) {
      return false;
    }

    this.#weakMap.delete(channel);
    this.#refMap.delete(channel.channelId);
    this.#finalizationRegistry.unregister(entry.ref);
    return true;
  }

  /**
   * Retrieve a value stored in the ChannelMap by the provided channel.
   *
   * @param {nsIChannel} channel
   *     The key to delete from the ChannelMap.
   *
   * @return {Object|null}
   *     The value held for the provided channel.
   *     Null if the channel did not match any known key.
   */
  get(channel) {
    const ref = this.#refMap.get(channel.channelId);
    const key = ref ? ref.deref() : null;
    if (!key) {
      return null;
    }
    const channelInfo = this.#weakMap.get(key);
    return channelInfo ? channelInfo.value : null;
  }

  /**
   * Adds or updates an entry in the ChannelMap for the provided channel.
   *
   * @param {nsIChannel} channel
   *     The key of the entry to add or update.
   * @param {Object} value
   *     The value to add or update.
   */
  set(channel, value) {
    const ref = new WeakRef(channel);
    this.#weakMap.set(channel, { value, ref });
    this.#refMap.set(channel.channelId, ref);
    this.#finalizationRegistry.register(
      channel,
      {
        refMap: this.#refMap,
        id: channel.channelId,
      },
      ref
    );
  }
}
