/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file offers standalone (no dependencies on other files) EME test
// helpers. The intention is that this file can be used to provide helpers
// while not coupling tests as tightly to `dom/media/test/manifest.js` or other
// files. This allows these helpers to be used in different tests across the
// codebase without imports becoming a mess.

// A helper class to assist in setting up EME on media.
//
// Usage
// 1. First configure the EME helper so it can have the information needed
//    to setup EME correctly. This is done by setting
//    - keySystem via `SetKeySystem`.
//    - initDataTypes via `SetInitDataTypes`.
//    - audioCapabilities and/or videoCapabilities via `SetAudioCapabilities`
//      and/or `SetVideoCapabilities`.
//    - keyIds and keys via `AddKeyIdAndKey`.
//    - onerror should be set to a function that will handle errors from the
//      helper. This function should take one argument, the error.
// 2. Use the helper to configure a media element via `ConfigureEme`.
// 3. One the promise from `ConfigureEme` has resolved the media element should
//    be configured and can be played. Errors that happen after this point are
//    reported via `onerror`.
var EmeHelper = class EmeHelper {
  // Members used to configure EME.
  _keySystem;
  _initDataTypes;
  _audioCapabilities = [];
  _videoCapabilities = [];

  // Map of keyIds to keys.
  _keyMap = new Map();

  // Will be called if an error occurs during event handling. Users of the
  // class should set a handler to be notified of errors.
  onerror;

  /**
   * Get the clearkey key system string.
   * @return The clearkey key system string.
   */
  static GetClearkeyKeySystemString() {
    return "org.w3.clearkey";
  }

  // Begin conversion helpers.

  /**
   * Helper to convert Uint8Array into base64 using base64url alphabet, without
   * padding.
   * @param uint8Array An array of bytes to convert to base64.
   * @return A base 64 encoded string
   */
  static Uint8ArrayToBase64(uint8Array) {
    return new TextDecoder()
      .decode(uint8Array)
      .replace(/\+/g, "-") // Replace chars for base64url.
      .replace(/\//g, "_")
      .replace(/=*$/, ""); // Remove padding for base64url.
  }

  /**
   * Helper to convert a hex string into base64 using base64url alphabet,
   * without padding.
   * @param hexString A string of hex characters.
   * @return A base 64 encoded string
   */
  static HexToBase64(hexString) {
    return btoa(
      hexString
        .match(/\w{2}/g) // Take chars two by two.
        // Map to characters.
        .map(hexByte => String.fromCharCode(parseInt(hexByte, 16)))
        .join("")
    )
      .replace(/\+/g, "-") // Replace chars for base64url.
      .replace(/\//g, "_")
      .replace(/=*$/, ""); // Remove padding for base64url.
  }

  /**
   * Helper to convert a base64 string (base64 or base64url) into a hex string.
   * @param base64String A base64 encoded string. This can be base64url.
   * @return A hex string (lower case);
   */
  static Base64ToHex(base64String) {
    let binString = atob(base64String.replace(/-/g, "+").replace(/_/g, "/"));
    let hexString = "";
    for (let i = 0; i < binString.length; i++) {
      // Covert to hex char. The "0" + and substr code are used to ensure we
      // always get 2 chars, even for outputs the would normally be only one.
      // E.g. for charcode 14 we'd get output 'e', and want to buffer that
      // to '0e'.
      hexString += ("0" + binString.charCodeAt(i).toString(16)).substr(-2);
    }
    // EMCA spec says that the num -> string conversion is lower case, so our
    // hex string should already be lower case.
    // https://tc39.es/ecma262/#sec-number.prototype.tostring
    return hexString;
  }

  // End conversion helpers.

  // Begin setters that setup the helper.
  // These should be used to configure the helper prior to calling
  // `ConfigureEme`.

  /**
   * Sets the key system that will be used by the EME helper.
   * @param keySystem The key system to use. Probably "org.w3.clearkey", which
   * can be fetched via `GetClearkeyKeySystemString`.
   */
  SetKeySystem(keySystem) {
    this._keySystem = keySystem;
  }

  /**
   * Sets the init data types that will be used by the EME helper. This is used
   * when calling `navigator.requestMediaKeySystemAccess`.
   * @param initDataTypes A list containing the init data types to be set by
   * the helper. This will usually be ["cenc"] or ["webm"], see
   * https://www.w3.org/TR/eme-initdata-registry/ for more info on what these
   * mean.
   */
  SetInitDataTypes(initDataTypes) {
    this._initDataTypes = initDataTypes;
  }

  /**
   * Sets the audio capabilities that will be used by the EME helper. These are
   * used when calling `navigator.requestMediaKeySystemAccess`.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Navigator/requestMediaKeySystemAccess
   * for more info on these.
   * @param audioCapabilities A list containing audio capabilities. E.g.
   * [{ contentType: 'audio/webm; codecs="opus"' }].
   */
  SetAudioCapabilities(audioCapabilities) {
    this._audioCapabilities = audioCapabilities;
  }

  /**
   * Sets the video capabilities that will be used by the EME helper. These are
   * used when calling `navigator.requestMediaKeySystemAccess`.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Navigator/requestMediaKeySystemAccess
   * for more info on these.
   * @param videoCapabilities A list containing video capabilities. E.g.
   * [{ contentType: 'video/webm; codecs="vp9"' }]
   */
  SetVideoCapabilities(videoCapabilities) {
    this._videoCapabilities = videoCapabilities;
  }

  /**
   * Adds a key id and key pair to the key map. These should both be hex
   * strings. E.g.
   * emeHelper.AddKeyIdAndKey(
   *   "2cdb0ed6119853e7850671c3e9906c3c",
   *   "808b9adac384de1e4f56140f4ad76194"
   * );
   * This function will store the keyId and key in lower case to ensure
   * consistency internally.
   * @param keyId The key id used to lookup the following key.
   * @param key The key associated with the earlier key id.
   */
  AddKeyIdAndKey(keyId, key) {
    this._keyMap.set(keyId.toLowerCase(), key.toLowerCase());
  }

  /**
   * Removes a key id and its associate key from the key map.
   * @param keyId The key id to remove.
   */
  RemoveKeyIdAndKey(keyId) {
    this._keyMap.delete(keyId);
  }

  // End setters that setup the helper.

  /**
   * Internal handler for `session.onmessage`. When calling this either do so
   * from inside an arrow function or using `bind` to ensure `this` points to
   * an EmeHelper instance (rather than a session).
   * @param messageEvent The message event passed to `session.onmessage`.
   */
  _SessionMessageHandler(messageEvent) {
    // This handles a session message and generates a clearkey license based
    // on the information in this._keyMap. This is done by populating the
    // appropriate keys on the session based on the keyIds surfaced in the
    // session message (a license request).
    let request = JSON.parse(new TextDecoder().decode(messageEvent.message));

    let keys = [];
    for (const keyId of request.kids) {
      let id64 = keyId;
      let idHex = EmeHelper.Base64ToHex(keyId);
      let key = this._keyMap.get(idHex);

      if (key) {
        keys.push({
          kty: "oct",
          kid: id64,
          k: EmeHelper.HexToBase64(key),
        });
      }
    }

    let license = new TextEncoder().encode(
      JSON.stringify({
        keys,
        type: request.type || "temporary",
      })
    );

    let session = messageEvent.target;
    session.update(license).catch(error => {
      if (this.onerror) {
        this.onerror(error);
      } else {
        console.log(
          `EmeHelper got an error, but no onerror handler was registered! Logging to console, error: ${error}`
        );
      }
    });
  }

  /**
   * Configures EME on a media element using the parameters already set on the
   * instance of EmeHelper.
   * @param htmlMediaElement - A media element to configure EME on.
   * @return A promise that will be resolved once the media element is
   * configured. This promise will be rejected with an error if configuration
   * fails.
   */
  async ConfigureEme(htmlMediaElement) {
    if (!this._keySystem) {
      throw new Error("EmeHelper needs _keySystem to configure media");
    }
    if (!this._initDataTypes) {
      throw new Error("EmeHelper needs _initDataTypes to configure media");
    }
    if (!this._audioCapabilities.length && !this._videoCapabilities.length) {
      throw new Error(
        "EmeHelper needs _audioCapabilities or _videoCapabilities to configure media"
      );
    }
    const options = [
      {
        initDataTypes: this._initDataTypes,
        audioCapabilities: this._audioCapabilities,
        videoCapabilities: this._videoCapabilities,
      },
    ];
    let access = await window.navigator.requestMediaKeySystemAccess(
      this._keySystem,
      options
    );
    let mediaKeys = await access.createMediaKeys();
    await htmlMediaElement.setMediaKeys(mediaKeys);

    htmlMediaElement.onencrypted = async encryptedEvent => {
      let session = htmlMediaElement.mediaKeys.createSession();
      // Use arrow notation so that `this` is the EmeHelper in the message
      // handler. If we do `session.onmessage = this._SessionMessageHandler`
      // then `this` will be the session in the callback.
      session.onmessage = messageEvent =>
        this._SessionMessageHandler(messageEvent);
      try {
        await session.generateRequest(
          encryptedEvent.initDataType,
          encryptedEvent.initData
        );
      } catch (error) {
        if (this.onerror) {
          this.onerror(error);
        } else {
          console.log(
            `EmeHelper got an error, but no onerror handler was registered! Logging to console, error: ${error}`
          );
        }
      }
    };
  }
};
