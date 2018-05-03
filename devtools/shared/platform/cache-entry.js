/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

 const Services = require("Services");
 const {Ci} = require("chrome");
 loader.lazyRequireGetter(this, "NetworkHelper",
                         "devtools/shared/webconsole/network-helper");

 /**
  * Module to fetch cache objects from CacheStorageService
  * and return them as an object.
  */
 exports.CacheEntry = {
  /**
   * Flag for cache session being initialized.
   */
   isCacheSessionInitialized: false,
  /**
   * Cache session object.
   */
   cacheSession: null,

  /**
   * Initializes our cache session / cache storage session.
   */
   initializeCacheSession: function(request) {
     try {
       let cacheService = Services.cache2;
       if (cacheService) {
         let loadContext = NetworkHelper.getRequestLoadContext(request);
         if (!loadContext) { // Get default load context if we can't fetch.
           loadContext = Services.loadContextInfo.default;
         }
         this.cacheSession =
           cacheService.diskCacheStorage(loadContext, false);
         this.isCacheSessionInitialized = true;
       }
     } catch (e) {
       this.isCacheSessionInitialized = false;
     }
   },

  /**
   * Parses a cache descriptor returned from the backend into a
   * usable object.
   *
   * @param Object descriptor The descriptor from the backend.
   */
   parseCacheDescriptor: function(descriptor) {
     let descriptorObj = {};
     try {
       if (descriptor.storageDataSize) {
         descriptorObj.dataSize = descriptor.storageDataSize;
       }
     } catch (e) {
      // We just need to handle this in case it's a js file of 0B.
     }
     if (descriptor.expirationTime) {
       descriptorObj.expires = descriptor.expirationTime;
     }
     if (descriptor.fetchCount) {
       descriptorObj.fetchCount = descriptor.fetchCount;
     }
     if (descriptor.lastFetched) {
       descriptorObj.lastFetched = descriptor.lastFetched;
     }
     if (descriptor.lastModified) {
       descriptorObj.lastModified = descriptor.lastModified;
     }
     if (descriptor.deviceID) {
       descriptorObj.device = descriptor.deviceID;
     }
     return descriptorObj;
   },

  /**
   * Does the fetch for the cache descriptor from the session.
   *
   * @param string request
   *        The request object.
   * @param Function onCacheDescriptorAvailable
   *        callback function.
   */
   getCacheEntry: function(request, onCacheDescriptorAvailable) {
     if (!this.isCacheSessionInitialized) {
       this.initializeCacheSession(request);
     }
     if (this.cacheSession) {
       let uri = NetworkHelper.nsIURL(request.URI.spec);
       this.cacheSession.asyncOpenURI(
         uri,
        "",
        Ci.nsICacheStorage.OPEN_SECRETLY,
         {
           onCacheEntryCheck: (entry, appcache) => {
             return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
           },
           onCacheEntryAvailable: (descriptor, isnew, appcache, status) => {
             if (descriptor) {
               let descriptorObj = this.parseCacheDescriptor(descriptor);
               onCacheDescriptorAvailable(descriptorObj);
             } else {
               onCacheDescriptorAvailable(null);
             }
           }
         }
       );
     } else {
       onCacheDescriptorAvailable(null);
     }
   }
 };
