"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Components */

var loop = loop || {};
loop.shared = loop.shared || {};
var inChrome = typeof Components != "undefined" && 
"utils" in Components;

// The slideshow is special, and currently loads with chrome privs, but
// needs to use this module like the rest of the content does.  Once we make
// it load remotely, this can go away.
if (inChrome) {
  if (typeof window != "undefined" && 
  window.location.href === "chrome://loop/content/panels/slideshow.html") {

    inChrome = false;}}



(function () {
  "use strict";

  /**
   * Root object, by default set to window if we're not in chrome code.
   * @type {DOMWindow|Object}
   */
  var rootObject = inChrome ? {} : window;
  /**
   * Root navigator, by default set to navigator if we're not in chrome code.
   * @type {Navigator|Object}
   */
  var rootNavigator = inChrome ? {} : navigator;

  /**
   * Sets new root objects.  This is useful for testing native DOM events, and also
   * so we can easily stub items like navigator.mediaDevices.
   * can fake them. In beforeEach(), loop.shared.utils.setRootObjects is used to
   * substitute fake objects, and in afterEach(), the real window object is
   * replaced.
   *
   * @param {Object} windowObj The fake window object, undefined to use window.
   * @param {Object} navigatorObj The fake navigator object, undefined to use navigator.
   */
  function setRootObjects(windowObj, navigatorObj) {
    rootObject = windowObj || window;
    rootNavigator = navigatorObj || navigator;}


  var mozL10n;
  if (inChrome) {
    this.EXPORTED_SYMBOLS = ["utils"];
    mozL10n = { get: function get() {
        throw new Error("mozL10n.get not availabled from chrome!");} };} else 

  {
    mozL10n = document.mozL10n || navigator.mozL10n;}


  /**
   * Call types used for determining if a call is audio/video or audio-only.
   */
  var CALL_TYPES = { 
    AUDIO_VIDEO: "audio-video", 
    AUDIO_ONLY: "audio" };


  var REST_ERRNOS = { 
    INVALID_TOKEN: 105, 
    EXPIRED: 111, 
    USER_UNAVAILABLE: 122, 
    ROOM_FULL: 202 };


  var FAILURE_DETAILS = { 
    MEDIA_DENIED: "reason-media-denied", 
    NO_MEDIA: "reason-no-media", 
    ROOM_ALREADY_OPEN: "reason-room-already-open", 
    UNABLE_TO_PUBLISH_MEDIA: "unable-to-publish-media", 
    USER_UNAVAILABLE: "reason-user-unavailable", 
    COULD_NOT_CONNECT: "reason-could-not-connect", 
    NETWORK_DISCONNECTED: "reason-network-disconnected", 
    EXPIRED_OR_INVALID: "reason-expired-or-invalid", 
    // TOS_FAILURE reflects the sdk error code 1026:
    // https://tokbox.com/developer/sdks/js/reference/ExceptionEvent.html
    TOS_FAILURE: "reason-tos-failure", 
    UNKNOWN: "reason-unknown", 
    ICE_FAILED: "reason-ice-failed" };


  var ROOM_INFO_FAILURES = { 
    // There's no data available from the server.
    NO_DATA: "no_data", 
    // WebCrypto is unsupported in this browser.
    WEB_CRYPTO_UNSUPPORTED: "web_crypto_unsupported", 
    // The room is missing the crypto key information.
    NO_CRYPTO_KEY: "no_crypto_key", 
    // Decryption failed.
    DECRYPT_FAILED: "decrypt_failed" };


  var STREAM_PROPERTIES = { 
    VIDEO_DIMENSIONS: "videoDimensions", 
    HAS_AUDIO: "hasAudio", 
    HAS_VIDEO: "hasVideo" };


  var SCREEN_SHARE_STATES = { 
    INACTIVE: "ss-inactive", 
    // Pending is when the user is being prompted, aka gUM in progress.
    PENDING: "ss-pending", 
    ACTIVE: "ss-active" };


  var CHAT_CONTENT_TYPES = { 
    CONTEXT: "chat-context", 
    TEXT: "chat-text", 
    CONTEXT_TILE: "context-tile", 
    NOTIFICATION: "chat-notification" };


  var CURSOR_MESSAGE_TYPES = { 
    POSITION: "cursor-position", 
    CLICK: "cursor-click" };


  /**
   * Format a given date into an l10n-friendly string.
   *
   * @param {Integer} The timestamp in seconds to format.
   * @return {String} The formatted string.
   */
  function formatDate(timestamp) {
    var date = new Date(timestamp * 1000);
    var options = { year: "numeric", month: "long", day: "numeric" };
    return date.toLocaleDateString(navigator.language, options);}


  /**
   * Check to see if we're currently on the desktop client or the standalone,
   * link-clicker page instead.
   * Note: we check for 'sendAsyncMessage' here too to prevent the ui-showcase
   *       to start playing audio.
   *
   * @return {Boolean}
   */
  function isDesktop() {
    return rootObject.document.body.className.indexOf("standalone") === -1 && 
    "sendAsyncMessage" in rootObject;}


  /**
   * Used for getting a boolean preference. It will either use the browser preferences
   * or try to get them from localStorage.
   *
   * @param {String}   prefName The name of the preference. Note that mozLoop
   *                            adds 'loop.' to the start of the string.
   * @param {Function} callback Invoked with the preference value, coerced to type
   *                            Boolean if necessary.
   */
  function getBoolPreference(prefName, callback) {
    if (loop.shared.utils.isDesktop()) {
      loop.request("GetLoopPref", prefName).then(function (result) {
        callback(!!result);});

      return;}


    callback(!!localStorage.getItem(prefName));}


  function isChrome(platform) {
    return platform.toLowerCase().indexOf("chrome") > -1 || 
    platform.toLowerCase().indexOf("chromium") > -1;}


  function isFirefox(platform) {
    return platform.toLowerCase().indexOf("firefox") !== -1;}


  function isOpera(platform) {
    return platform.toLowerCase().indexOf("opera") > -1 || 
    platform.toLowerCase().indexOf("opr") > -1;}


  /**
   * Helper to get the platform if it is unsupported.
   *
   * @param {String} platform The platform this is running on.
   * @return null for supported platforms, a string for unsupported platforms.
   */
  function getUnsupportedPlatform(platform) {
    if (/^(iPad|iPhone|iPod)/.test(platform)) {
      return "ios";}


    if (/Windows Phone/i.test(platform)) {
      return "windows_phone";}


    if (/BlackBerry/i.test(platform)) {
      return "blackberry";}


    // Checks if the platform is Android. Due to the difficulties of detecting an
    // android device, we need to rely on window.navigator.userAgent instead of
    // using window.navigator.platform.
    if (rootNavigator.userAgent.toLowerCase().indexOf("android") > -1) {
      return "android";}


    return null;}


  /**
   * Helper to get the Operating System name.
   *
   * @param {String}  [platform]    The platform this is running on, will fall
   *                                back to navigator.oscpu and navigator.userAgent
   *                                respectively if not supplied.
   * @param {Boolean} [withVersion] Optional flag to keep the version number
   *                                included in the resulting string. Defaults to
   *                                `false`.
   * @return {String} The platform we're currently running on, in lower-case.
   */
  var getOS = function getOS(platform, withVersion) {
    if (!platform) {
      if ("oscpu" in window.navigator) {
        // See https://developer.mozilla.org/en-US/docs/Web/API/Navigator/oscpu
        platform = window.navigator.oscpu.split(";")[0].trim();} else 
      {
        // Fall back to navigator.userAgent as a last resort.
        platform = window.navigator.userAgent;}}



    if (!platform) {
      return "unknown";}


    // Support passing in navigator.userAgent.
    var platformPart = platform.match(/\((.*)\)/);
    if (platformPart) {
      platform = platformPart[1];}

    platform = platform.toLowerCase().split(";");
    if (/macintosh/.test(platform[0]) || /x11/.test(platform[0])) {
      platform = platform[1];} else 
    if (platform[0].indexOf("win") > -1 && platform.length > 4) {
      // Skip the security notation.
      platform = platform[2];} else 
    {
      platform = platform[0];}


    if (!withVersion) {
      platform = platform.replace(/\s[0-9.]+/g, "");}


    return platform.trim();};


  /**
   * Helper to get the Operating System version.
   * See http://en.wikipedia.org/wiki/Windows_NT for a table of Windows NT
   * versions.
   *
   * @param {String} [platform] The platform this is running on, will fall back
   *                            to navigator.oscpu and navigator.userAgent
   *                            respectively if not supplied.
   * @return {String} The current version of the platform we're currently running
   *                  on.
   */
  var getOSVersion = function getOSVersion(platform) {
    var os = getOS(platform, true);
    var digitsRE = /\s([0-9.]+)/;

    var version = os.match(digitsRE);
    if (!version) {
      if (os.indexOf("win") > -1) {
        if (os.indexOf("xp")) {
          return { major: 5, minor: 2 };} else 
        if (os.indexOf("vista") > -1) {
          return { major: 6, minor: 0 };}}} else 


    {
      version = version[1];
      // Windows versions have an interesting scheme.
      if (os.indexOf("win") > -1) {
        switch (parseFloat(version)) {
          case 98:
            return { major: 4, minor: 1 };
          case 2000:
            return { major: 5, minor: 0 };
          case 2003:
            return { major: 5, minor: 2 };
          case 7:
          case 2008:
          case 2011:
            return { major: 6, minor: 1 };
          case 8:
            return { major: 6, minor: 2 };
          case 8.1:
          case 2012:
            return { major: 6, minor: 3 };}}



      version = version.split(".");
      return { 
        major: parseInt(version[0].trim(), 10), 
        minor: parseInt(version[1] ? version[1].trim() : 0, 10) };}



    return { major: Infinity, minor: 0 };};


  /**
   * Helper to get the current short platform string, based on the return value
   * of `getOS`.
   * Possible return values are 'mac', 'win' or 'other'.
   *
   * @param  {String} [os] Optional string for the OS, used in tests only.
   * @return {String} 'mac', 'win' or 'other'.
   */
  var getPlatform = function getPlatform(os) {
    os = getOS(os);
    var platform = "other";
    if (os.indexOf("mac") > -1) {
      platform = "mac";} else 
    if (os.indexOf("win") > -1) {
      platform = "win";}

    return platform;};


  /**
   * Determines if the user has any audio devices installed.
   *
   * @param  {Function} callback Called with a boolean which is true if there
   *                             are audio devices present.
   */
  function hasAudioOrVideoDevices(callback) {
    // mediaDevices is the official API for the spec.
    // Older versions of FF had mediaDevices but not enumerateDevices.
    if ("mediaDevices" in rootNavigator && 
    "enumerateDevices" in rootNavigator.mediaDevices) {
      rootNavigator.mediaDevices.enumerateDevices().then(function (result) {
        function checkForInput(device) {
          return device.kind === "audioinput" || device.kind === "videoinput";}


        callback(result.some(checkForInput));}).
      catch(function () {
        callback(false);});

      // MediaStreamTrack is the older version of the API, implemented originally
      // by Google Chrome.
    } else if ("MediaStreamTrack" in rootObject && 
    "getSources" in rootObject.MediaStreamTrack) {
      rootObject.MediaStreamTrack.getSources(function (result) {
        function checkForInput(device) {
          return device.kind === "audio" || device.kind === "video";}


        callback(result.some(checkForInput));});} else 

    {
      // We don't know, so assume true.
      callback(true);}}



  /**
   * Helper to allow getting some of the location data in a way that's compatible
   * with stubbing for unit tests.
   */
  function locationData() {
    return { 
      hash: window.location.hash, 
      pathname: window.location.pathname };}



  /**
   * Formats a url for display purposes. This includes converting the
   * domain to punycode, and then decoding the url.
   * Intended to be used for both display and (uglier in confusing cases) clickthrough,
   * as described by dveditz in comment 12 of the bug 1196143,
   * as well as testing the behavior case in the browser.
   *
   * @param {String}  url                   The url to format.
   * @param {String}  suppressConsoleError  For testing, call with a boolean which is true to squash the default console error.
   * @return {Object}                       An object containing the hostname,
   *                                        full location and protocol.
   */
  function formatURL(url, suppressConsoleError) {
    // We're using new URL to pass this through the browser's ACE/punycode
    // processing system. If the browser considers a url to need to be
    // punycode encoded for it to be displayed, then new URL will do that for
    // us. This saves us needing our own punycode library.
    // Note that URL does canonicalize hostname-only URLs,
    // adding a slash to them, but this is ok for at least HTTP(S)
    // because GET always has to specify a path, which will (by default) be
    var urlObject;
    try {
      urlObject = new URL(url);
      // Finally, ensure we look good.
      return { 
        hostname: urlObject.hostname, 
        location: decodeURI(urlObject.href), 
        protocol: urlObject.protocol };} 

    catch (ex) {
      if (suppressConsoleError ? !suppressConsoleError : true) {
        console.log("Error occurred whilst parsing URL: ", ex);
        console.trace();}

      return null;}}




  /**
   * Formats a url for context url links.
   *
   * @param {String}  url                   The url to format.
   * @return {Object}                       Sanitized url object containing the hostname,
   *                                        full location and protocol
   */
  function formatSanitizedContextURL(url) {
    if (!url) {
      return null;}

    // Bug 1196143 - formatURL sanitizes(decodes) the URL from IDN homographic attacks.
    // Try catch to not produce output if invalid url
    var sanitizedURL;
    try {
      sanitizedURL = loop.shared.utils.formatURL(url, true);} 
    catch (ex) {
      return null;}


    // Only allow specific types of URLs.
    if (!sanitizedURL || 
    sanitizedURL.protocol !== "http:" && 
    sanitizedURL.protocol !== "https:" && 
    sanitizedURL.protocol !== "ftp:") {
      return null;}


    return sanitizedURL;}


  /**
   * Generates and opens a mailto: url with call URL information prefilled.
   * Note: This only works for Desktop.
   *
   * @param {String} callUrl              The call URL.
   * @param {String} [recipient]          The recipient email address (optional).
   * @param {String} [contextDescription] The context description (optional).
   */
  function composeCallUrlEmail(callUrl, recipient, contextDescription) {
    if (!isDesktop()) {
      console.warn("composeCallUrlEmail isn't available for Loop standalone.");
      return;}


    var subject, body;
    var footer = mozL10n.get("share_email_footer2");
    if (contextDescription) {
      subject = mozL10n.get("share_email_subject7");
      body = mozL10n.get("share_email_body_context3", { 
        callUrl: callUrl, 
        title: contextDescription });} else 

    {
      subject = mozL10n.get("share_email_subject7");
      body = mozL10n.get("share_email_body7", { 
        callUrl: callUrl });}


    var bodyFooter = body + footer;
    bodyFooter = bodyFooter.replace(/\r\n/g, "\n").replace(/\n/g, "\r\n");
    loop.request("ComposeEmail", subject, bodyFooter, recipient);}


  // We can alias `subarray` to `slice` when the latter is not available, because
  // they're semantically identical.
  if (!Uint8Array.prototype.slice) {
    /* eslint-disable */
    // Eslint disabled for no-extend-native; Specific override needed for Firefox 37
    // and earlier, also for other browsers.
    Uint8Array.prototype.slice = Uint8Array.prototype.subarray;
    /* eslint-enable */}


  /**
   * Binary-compatible Base64 decoding.
   *
   * Taken from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
   *
   * @param {String} base64str The string to decode.
   * @return {Uint8Array} The decoded result in array format.
   */
  function atob(base64str) {
    var strippedEncoding = base64str.replace(/[^A-Za-z0-9\+\/]/g, "");
    var inLength = strippedEncoding.length;
    var outLength = inLength * 3 + 1 >> 2;
    var result = new Uint8Array(outLength);

    var mod3;
    var mod4;
    var uint24 = 0;
    var outIndex = 0;

    for (var inIndex = 0; inIndex < inLength; inIndex++) {
      mod4 = inIndex & 3;
      uint24 |= _b64ToUint6(strippedEncoding.charCodeAt(inIndex)) << 6 * (3 - mod4);

      if (mod4 === 3 || inLength - inIndex === 1) {
        for (mod3 = 0; mod3 < 3 && outIndex < outLength; mod3++, outIndex++) {
          result[outIndex] = uint24 >>> (16 >>> mod3 & 24) & 255;}

        uint24 = 0;}}



    return result;}


  /**
   * Binary-compatible Base64 encoding.
   *
   * Taken from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
   *
   * @param {Uint8Array} bytes The data to encode.
   * @return {String} The base64 encoded string.
   */
  function btoa(bytes) {
    var mod3 = 2;
    var result = "";
    var length = bytes.length;
    var uint24 = 0;

    for (var index = 0; index < length; index++) {
      mod3 = index % 3;
      if (index > 0 && index * 4 / 3 % 76 === 0) {
        result += "\r\n";}

      uint24 |= bytes[index] << (16 >>> mod3 & 24);
      if (mod3 === 2 || length - index === 1) {
        result += String.fromCharCode(_uint6ToB64(uint24 >>> 18 & 63), 
        _uint6ToB64(uint24 >>> 12 & 63), 
        _uint6ToB64(uint24 >>> 6 & 63), 
        _uint6ToB64(uint24 & 63));
        uint24 = 0;}}



    return result.substr(0, result.length - 2 + mod3) + (
    mod3 === 2 ? "" : mod3 === 1 ? "=" : "==");}


  /**
   * Utility function to decode a base64 character into an integer.
   *
   * Taken from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
   *
   * @param {Number} chr The character code to decode.
   * @return {Number} The decoded value.
   */
  function _b64ToUint6(chr) {
    return chr > 64 && chr < 91 ? chr - 65 : 
    chr > 96 && chr < 123 ? chr - 71 : 
    chr > 47 && chr < 58 ? chr + 4 : 
    chr === 43 ? 62 : 
    chr === 47 ? 63 : 0;}


  /**
   * Utility function to encode an integer into a base64 character code.
   *
   * Taken from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
   *
   * @param {Number} uint6 The number to encode.
   * @return {Number} The encoded value.
   */
  function _uint6ToB64(uint6) {
    return uint6 < 26 ? uint6 + 65 : 
    uint6 < 52 ? uint6 + 71 : 
    uint6 < 62 ? uint6 - 4 : 
    uint6 === 62 ? 43 : 
    uint6 === 63 ? 47 : 65;}


  /**
   * Utility function to convert a string into a uint8 array.
   *
   * Taken from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
   *
   * @param {String} inString The string to convert.
   * @return {Uint8Array} The converted string in array format.
   */
  function strToUint8Array(inString) {
    var inLength = inString.length;
    var arrayLength = 0;
    var chr;

    // Mapping.
    for (var mapIndex = 0; mapIndex < inLength; mapIndex++) {
      chr = inString.charCodeAt(mapIndex);
      arrayLength += chr < 0x80 ? 1 : 
      chr < 0x800 ? 2 : 
      chr < 0x10000 ? 3 : 
      chr < 0x200000 ? 4 : 
      chr < 0x4000000 ? 5 : 6;}


    var result = new Uint8Array(arrayLength);
    var index = 0;

    // Transcription.
    for (var chrIndex = 0; index < arrayLength; chrIndex++) {
      chr = inString.charCodeAt(chrIndex);
      if (chr < 128) {
        // One byte.
        result[index++] = chr;} else 
      if (chr < 0x800) {
        // Two bytes.
        result[index++] = 192 + (chr >>> 6);
        result[index++] = 128 + (chr & 63);} else 
      if (chr < 0x10000) {
        // Three bytes.
        result[index++] = 224 + (chr >>> 12);
        result[index++] = 128 + (chr >>> 6 & 63);
        result[index++] = 128 + (chr & 63);} else 
      if (chr < 0x200000) {
        // Four bytes.
        result[index++] = 240 + (chr >>> 18);
        result[index++] = 128 + (chr >>> 12 & 63);
        result[index++] = 128 + (chr >>> 6 & 63);
        result[index++] = 128 + (chr & 63);} else 
      if (chr < 0x4000000) {
        // Five bytes.
        result[index++] = 248 + (chr >>> 24);
        result[index++] = 128 + (chr >>> 18 & 63);
        result[index++] = 128 + (chr >>> 12 & 63);
        result[index++] = 128 + (chr >>> 6 & 63);
        result[index++] = 128 + (chr & 63);} else 
      {// if (chr <= 0x7fffffff)
        // Six bytes.
        result[index++] = 252 + (chr >>> 30);
        result[index++] = 128 + (chr >>> 24 & 63);
        result[index++] = 128 + (chr >>> 18 & 63);
        result[index++] = 128 + (chr >>> 12 & 63);
        result[index++] = 128 + (chr >>> 6 & 63);
        result[index++] = 128 + (chr & 63);}}



    return result;}


  /**
   * Utility function to change a uint8 based integer array to a string.
   *
   * Taken from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
   *
   * @param {Uint8Array} arrayBytes Array to convert.
   * @param {String} The array as a string.
   */
  function Uint8ArrayToStr(arrayBytes) {
    var result = "";
    var length = arrayBytes.length;
    var part;

    for (var index = 0; index < length; index++) {
      part = arrayBytes[index];
      result += String.fromCharCode(
      part > 251 && part < 254 && index + 5 < length ? 
      // Six bytes.
      // (part - 252 << 30) may be not so safe in ECMAScript! So...:
      (part - 252) * 1073741824 + (
      arrayBytes[++index] - 128 << 24) + (
      arrayBytes[++index] - 128 << 18) + (
      arrayBytes[++index] - 128 << 12) + (
      arrayBytes[++index] - 128 << 6) + 
      arrayBytes[++index] - 128 : 
      part > 247 && part < 252 && index + 4 < length ? 
      // Five bytes.
      (part - 248 << 24) + (
      arrayBytes[++index] - 128 << 18) + (
      arrayBytes[++index] - 128 << 12) + (
      arrayBytes[++index] - 128 << 6) + 
      arrayBytes[++index] - 128 : 
      part > 239 && part < 248 && index + 3 < length ? 
      // Four bytes.
      (part - 240 << 18) + (
      arrayBytes[++index] - 128 << 12) + (
      arrayBytes[++index] - 128 << 6) + 
      arrayBytes[++index] - 128 : 
      part > 223 && part < 240 && index + 2 < length ? 
      // Three bytes.
      (part - 224 << 12) + (
      arrayBytes[++index] - 128 << 6) + 
      arrayBytes[++index] - 128 : 
      part > 191 && part < 224 && index + 1 < length ? 
      // Two bytes.
      (part - 192 << 6) + 
      arrayBytes[++index] - 128 : 
      // One byte.
      part);}



    return result;}


  /**
   * Get the difference after comparing two different objects. It compares property
   * names and their respective values if necessary.
   * This function does _not_ recurse into object values to keep this functions'
   * complexity predictable to O(2).
   *
   * @param  {Object} a Object number 1, the comparator.
   * @param  {Object} b Object number 2, the comparison.
   * @return {Object}   The diff output, which is itself an object structured as:
   *                    {
   *                      updated: [prop1, prop6],
   *                      added: [prop2],
   *                      removed: [prop3]
   *                    }
   */
  function objectDiff(a, b) {
    var propsA = a ? Object.getOwnPropertyNames(a) : [];
    var propsB = b ? Object.getOwnPropertyNames(b) : [];
    var diff = { 
      updated: [], 
      added: [], 
      removed: [] };


    var prop;
    for (var i = 0, lA = propsA.length; i < lA; ++i) {
      prop = propsA[i];
      if (propsB.indexOf(prop) === -1) {
        diff.removed.push(prop);} else 
      if (a[prop] !== b[prop]) {
        diff.updated.push(prop);}}



    for (var j = 0, lB = propsB.length; j < lB; ++j) {
      prop = propsB[j];
      if (propsA.indexOf(prop) === -1) {
        diff.added.push(prop);}}



    return diff;}


  /**
   * When comparing two object, you sometimes want to ignore falsy values when
   * they're not persisted on the server, for example.
   * This function removes all the empty/ falsy properties from the target object.
   *
   * @param  {Object} obj Target object to strip the falsy properties from
   * @return {Object}
   */
  function stripFalsyValues(obj) {
    var props = Object.getOwnPropertyNames(obj);
    var prop;
    for (var i = props.length; i >= 0; --i) {
      prop = props[i];
      // If the value of the object property evaluates to |false|, delete it.
      if (!obj[prop]) {
        delete obj[prop];}}


    return obj;}


  /**
   * Look up the DOM hierarchy for a node matching `selector`.
   * If it is not found return the parent node, this is a sane default so
   * that subsequent queries on the result do no fail.
   * Better choice than the alternative `document.querySelector(selector)`
   * because we ensure it works in the UI showcase as well.
   *
   * @param {HTMLElement} node Child element of the node we are looking for.
   * @param {String} selector  CSS class value of element we are looking for.
   * @return {HTMLElement}     Parent of node that matches selector query.
   */
  function findParentNode(node, selector) {
    var parentNode = node.parentNode;

    while (parentNode) {
      if (parentNode.classList.contains(selector)) {
        return parentNode;}


      parentNode = parentNode.parentNode;}


    return node;}


  this.utils = { 
    CALL_TYPES: CALL_TYPES, 
    CHAT_CONTENT_TYPES: CHAT_CONTENT_TYPES, 
    CURSOR_MESSAGE_TYPES: CURSOR_MESSAGE_TYPES, 
    FAILURE_DETAILS: FAILURE_DETAILS, 
    REST_ERRNOS: REST_ERRNOS, 
    STREAM_PROPERTIES: STREAM_PROPERTIES, 
    SCREEN_SHARE_STATES: SCREEN_SHARE_STATES, 
    ROOM_INFO_FAILURES: ROOM_INFO_FAILURES, 
    setRootObjects: setRootObjects, 
    composeCallUrlEmail: composeCallUrlEmail, 
    findParentNode: findParentNode, 
    formatDate: formatDate, 
    formatSanitizedContextURL: formatSanitizedContextURL, 
    formatURL: formatURL, 
    getBoolPreference: getBoolPreference, 
    getOS: getOS, 
    getOSVersion: getOSVersion, 
    getPlatform: getPlatform, 
    isChrome: isChrome, 
    isDesktop: isDesktop, 
    isFirefox: isFirefox, 
    isOpera: isOpera, 
    getUnsupportedPlatform: getUnsupportedPlatform, 
    hasAudioOrVideoDevices: hasAudioOrVideoDevices, 
    locationData: locationData, 
    atob: atob, 
    btoa: btoa, 
    strToUint8Array: strToUint8Array, 
    Uint8ArrayToStr: Uint8ArrayToStr, 
    objectDiff: objectDiff, 
    stripFalsyValues: stripFalsyValues };}).

call(inChrome ? this : loop.shared);
