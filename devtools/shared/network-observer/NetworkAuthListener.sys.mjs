/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

/**
 * This class is a simplified version from the AuthRequestor used by the
 * WebExtensions codebase at:
 *   https://searchfox.org/mozilla-central/rev/fd2325f5b2a5be8f8f2acf9307285f2b7de06582/toolkit/components/extensions/webrequest/WebRequest.sys.mjs#434-579
 *
 * The NetworkAuthListener will monitor the provided channel and will invoke the
 * owner's `onAuthPrompt` end point whenever an auth challenge is requested.
 *
 * The owner will receive several callbacks to proceed with the prompt:
 * - cancelAuthPrompt(): cancel the authentication attempt
 * - forwardAuthPrompt(): forward the auth prompt request to the next
 *   notification callback. If no other custom callback is set, this will
 *   typically lead to show the auth prompt dialog in the browser UI.
 * - provideAuthCredentials(username, password): attempt to authenticate with
 *   the provided username and password.
 *
 * Please note that the request will be blocked until the consumer calls one of
 * the callbacks listed above. Make sure to eventually unblock the request if
 * you implement `onAuthPrompt`.
 *
 * @param {nsIChannel} channel
 *     The channel to monitor.
 * @param {object} owner
 *     The owner object, expected to implement `onAuthPrompt`.
 */
export class NetworkAuthListener {
  constructor(channel, owner) {
    this.notificationCallbacks = channel.notificationCallbacks;
    this.loadGroupCallbacks =
      channel.loadGroup && channel.loadGroup.notificationCallbacks;
    this.owner = owner;

    // Setup the channel's notificationCallbacks to be handled by this instance.
    channel.notificationCallbacks = this;
  }

  // See https://searchfox.org/mozilla-central/source/netwerk/base/nsIAuthPrompt2.idl
  asyncPromptAuth(channel, callback, context, level, authInfo) {
    const isProxy = !!(authInfo.flags & authInfo.AUTH_PROXY);
    const cancelAuthPrompt = () => {
      if (channel.canceled) {
        return;
      }

      try {
        callback.onAuthCancelled(context, false);
      } catch (e) {
        console.error(`NetworkAuthListener failed to cancel auth prompt ${e}`);
      }
    };

    const forwardAuthPrompt = () => {
      if (channel.canceled) {
        return;
      }

      const prompt = this.#getForwardPrompt(isProxy);
      prompt.asyncPromptAuth(channel, callback, context, level, authInfo);
    };

    const provideAuthCredentials = (username, password) => {
      if (channel.canceled) {
        return;
      }

      authInfo.username = username;
      authInfo.password = password;
      try {
        callback.onAuthAvailable(context, authInfo);
      } catch (e) {
        console.error(
          `NetworkAuthListener failed to provide auth credentials ${e}`
        );
      }
    };

    const authDetails = {
      isProxy,
      realm: authInfo.realm,
      scheme: authInfo.authenticationScheme,
    };
    const authCallbacks = {
      cancelAuthPrompt,
      forwardAuthPrompt,
      provideAuthCredentials,
    };

    // The auth callbacks may only be called asynchronously after this method
    // successfully returned.
    lazy.setTimeout(() => this.#notifyOwner(authDetails, authCallbacks), 1);

    return {
      QueryInterface: ChromeUtils.generateQI(["nsICancelable"]),
      cancel: cancelAuthPrompt,
    };
  }

  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPromptProvider) || iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }
    try {
      return this.notificationCallbacks.getInterface(iid);
    } catch (e) {}
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  }

  // See https://searchfox.org/mozilla-central/source/netwerk/base/nsIAuthPromptProvider.idl
  getAuthPrompt(reason, iid) {
    // This should never get called without getInterface having been called first.
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }
    return this.#getForwardedInterface(Ci.nsIAuthPromptProvider).getAuthPrompt(
      reason,
      iid
    );
  }

  // See https://searchfox.org/mozilla-central/source/netwerk/base/nsIAuthPrompt2.idl
  promptAuth(channel, level, authInfo) {
    this.#getForwardedInterface(Ci.nsIAuthPrompt2).promptAuth(
      channel,
      level,
      authInfo
    );
  }

  #getForwardedInterface(iid) {
    try {
      return this.notificationCallbacks.getInterface(iid);
    } catch (e) {
      return this.loadGroupCallbacks.getInterface(iid);
    }
  }

  #getForwardPrompt(isProxy) {
    const reason = isProxy
      ? Ci.nsIAuthPromptProvider.PROMPT_PROXY
      : Ci.nsIAuthPromptProvider.PROMPT_NORMAL;
    for (const callbacks of [
      this.notificationCallbacks,
      this.loadGroupCallbacks,
    ]) {
      try {
        return callbacks
          .getInterface(Ci.nsIAuthPromptProvider)
          .getAuthPrompt(reason, Ci.nsIAuthPrompt2);
      } catch (e) {}
      try {
        return callbacks.getInterface(Ci.nsIAuthPrompt2);
      } catch (e) {}
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  }

  #notifyOwner(authDetails, authCallbacks) {
    if (typeof this.owner.onAuthPrompt == "function") {
      this.owner.onAuthPrompt(authDetails, authCallbacks);
    } else {
      console.error(
        "NetworkObserver owner enabled the auth prompt listener " +
          "but does not implement 'onAuthPrompt'. " +
          "Forwarding the auth prompt to the next notification callback."
      );
      authCallbacks.forwardAuthPrompt();
    }
  }
}

NetworkAuthListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIInterfaceRequestor",
  "nsIAuthPromptProvider",
  "nsIAuthPrompt2",
]);
