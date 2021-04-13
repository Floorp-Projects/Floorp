/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, ExtensionParent, Services, XPCOMUtils */

XPCOMUtils.defineLazyGlobalGetters(this, ["URL", "ChannelWrapper"]);

class Manager {
  constructor() {
    this._allowLists = new Map();
  }

  _ensureStarted() {
    if (this._classifierObserver) {
      return;
    }

    this._unblockedChannelIds = new Set();
    this._channelClassifier = Cc[
      "@mozilla.org/url-classifier/channel-classifier-service;1"
    ].getService(Ci.nsIChannelClassifierService);
    this._classifierObserver = {};
    this._classifierObserver.observe = (subject, topic, data) => {
      switch (topic) {
        case "http-on-stop-request": {
          const { channelId } = subject.QueryInterface(Ci.nsIIdentChannel);
          this._unblockedChannelIds.delete(channelId);
          break;
        }
        case "urlclassifier-before-block-channel": {
          const channel = subject.QueryInterface(
            Ci.nsIUrlClassifierBlockedChannel
          );
          const { channelId, url } = channel;
          let topHost;
          try {
            topHost = new URL(channel.topLevelUrl).hostname;
          } catch (_) {
            return;
          }
          for (const allowList of this._allowLists.values()) {
            for (const entry of allowList.values()) {
              const { matcher, hosts, notHosts, willBeShimming } = entry;
              if (matcher.matches(url)) {
                if (
                  !notHosts?.has(topHost) &&
                  (hosts === true || hosts.has(topHost))
                ) {
                  this._unblockedChannelIds.add(channelId);
                  if (willBeShimming) {
                    channel.replace();
                  } else {
                    channel.allow();
                  }
                  return;
                }
              }
            }
          }
          break;
        }
      }
    };
    Services.obs.addObserver(this._classifierObserver, "http-on-stop-request");
    this._channelClassifier.addListener(this._classifierObserver);
  }

  stop() {
    if (!this._classifierObserver) {
      return;
    }

    Services.obs.removeObserver(
      this._classifierObserver,
      "http-on-stop-request"
    );
    this._channelClassifier.removeListener(this._classifierObserver);
    delete this._channelClassifier;
    delete this._classifierObserver;
  }

  wasChannelIdUnblocked(channelId) {
    return this._unblockedChannelIds?.has(channelId);
  }

  allow(allowListId, patterns, { hosts, notHosts, willBeShimming }) {
    this._ensureStarted();

    if (!this._allowLists.has(allowListId)) {
      this._allowLists.set(allowListId, new Map());
    }
    const allowList = this._allowLists.get(allowListId);
    for (const pattern of patterns) {
      if (!allowList.has(pattern)) {
        allowList.set(pattern, {
          matcher: new MatchPattern(pattern),
        });
      }
      const allowListPattern = allowList.get(pattern);
      allowListPattern.willBeShimming = willBeShimming;
      if (!hosts) {
        allowListPattern.hosts = true;
      } else if (allowListPattern.hosts !== true) {
        if (!allowListPattern.hosts) {
          allowListPattern.hosts = new Set();
        }
        for (const host of hosts) {
          allowListPattern.hosts.add(host);
        }
      }
      if (notHosts) {
        if (!allowListPattern.notHosts) {
          allowListPattern.notHosts = new Set();
        }
        for (const notHost of notHosts) {
          allowListPattern.notHosts.add(notHost);
        }
      }
    }
  }

  revoke(allowListId) {
    this._allowLists.delete(allowListId);
  }
}
var manager = new Manager();

function getChannelId(context, requestId) {
  const wrapper = ChannelWrapper.getRegisteredChannel(
    requestId,
    context.extension.policy,
    context.xulBrowser.frameLoader.remoteTab
  );
  return wrapper?.channel?.QueryInterface(Ci.nsIIdentChannel)?.channelId;
}

this.trackingProtection = class extends ExtensionAPI {
  onShutdown(isAppShutdown) {
    if (manager) {
      manager.stop();
    }
  }

  getAPI(context) {
    return {
      trackingProtection: {
        async allow(allowListId, patterns, options) {
          manager.allow(allowListId, patterns, options);
        },
        async revoke(allowListId) {
          manager.revoke(allowListId);
        },
        async wasRequestUnblocked(requestId) {
          if (!manager) {
            return false;
          }
          const channelId = getChannelId(context, requestId);
          if (!channelId) {
            return false;
          }
          return manager.wasChannelIdUnblocked(channelId);
        },
      },
    };
  }
};
