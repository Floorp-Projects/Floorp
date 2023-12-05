/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/#the-navigator-object
 * http://www.w3.org/TR/tracking-dnt/
 * http://www.w3.org/TR/geolocation-API/#geolocation_interface
 * http://www.w3.org/TR/battery-status/#navigatorbattery-interface
 * http://www.w3.org/TR/vibration/#vibration-interface
 * http://www.w3.org/2012/sysapps/runtime/#extension-to-the-navigator-interface-1
 * https://dvcs.w3.org/hg/gamepad/raw-file/default/gamepad.html#navigator-interface-extension
 * http://www.w3.org/TR/beacon/#sec-beacon-method
 * https://html.spec.whatwg.org/#navigatorconcurrenthardware
 * http://wicg.github.io/netinfo/#extensions-to-the-navigator-interface
 * https://w3c.github.io/webappsec-credential-management/#framework-credential-management
 * https://w3c.github.io/webdriver/webdriver-spec.html#interface
 * https://wicg.github.io/media-capabilities/#idl-index
 * https://w3c.github.io/mediasession/#idl-index
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

interface URI;

// https://html.spec.whatwg.org/#the-navigator-object
[HeaderFile="Navigator.h",
 Exposed=Window,
 InstrumentedProps=(canShare,
                    clearAppBadge,
                    setAppBadge,
                    share,
                    userActivation,
                    wakeLock)]
interface Navigator {
  // objects implementing this interface also implement the interfaces given below
};
Navigator includes NavigatorID;
Navigator includes NavigatorLanguage;
Navigator includes NavigatorOnLine;
Navigator includes NavigatorContentUtils;
Navigator includes NavigatorStorageUtils;
Navigator includes NavigatorConcurrentHardware;
Navigator includes NavigatorStorage;
Navigator includes NavigatorAutomationInformation;
Navigator includes NavigatorGPU;
Navigator includes GlobalPrivacyControl;

interface mixin NavigatorID {
  // WebKit/Blink/Trident/Presto support this (hardcoded "Mozilla").
  [Constant, Cached, Throws]
  readonly attribute DOMString appCodeName; // constant "Mozilla"
  [Constant, Cached]
  readonly attribute DOMString appName; // constant "Netscape"
  [Constant, Cached, Throws, NeedsCallerType]
  readonly attribute DOMString appVersion;
  [Pure, Cached, Throws, NeedsCallerType]
  readonly attribute DOMString platform;
  [Pure, Cached, Throws, NeedsCallerType]
  readonly attribute DOMString userAgent;
  [Constant, Cached]
  readonly attribute DOMString product; // constant "Gecko"

  // Everyone but WebKit/Blink supports this.  See bug 679971.
  [Exposed=Window]
  boolean taintEnabled(); // constant false
};

interface mixin NavigatorLanguage {

  // These two attributes are cached because this interface is also implemented
  // by Workernavigator and this way we don't have to go back to the
  // main-thread from the worker thread anytime we need to retrieve them. They
  // are updated when pref intl.accept_languages is changed.

  [Pure, Cached]
  readonly attribute DOMString? language;
  [Pure, Cached, Frozen]
  readonly attribute sequence<DOMString> languages;
};

interface mixin NavigatorOnLine {
  readonly attribute boolean onLine;
};

interface mixin NavigatorContentUtils {
  // content handler registration
  [Throws, ChromeOnly]
  undefined checkProtocolHandlerAllowed(DOMString scheme, URI handlerURI, URI documentURI);
  [Throws, SecureContext]
  undefined registerProtocolHandler(DOMString scheme, DOMString url);
  // NOT IMPLEMENTED
  //undefined unregisterProtocolHandler(DOMString scheme, DOMString url);
};

[SecureContext]
interface mixin NavigatorStorage {
  readonly attribute StorageManager storage;
};

interface mixin NavigatorStorageUtils {
  // NOT IMPLEMENTED
  //undefined yieldForStorageUpdates();
};

partial interface Navigator {
  [Throws]
  readonly attribute Permissions permissions;
};

partial interface Navigator {
  [Throws, SameObject]
  readonly attribute MimeTypeArray mimeTypes;
  [Throws, SameObject]
  readonly attribute PluginArray plugins;
  readonly attribute boolean pdfViewerEnabled;
};

// http://www.w3.org/TR/tracking-dnt/ sort of
partial interface Navigator {
  readonly attribute DOMString doNotTrack;
};

// https://globalprivacycontrol.github.io/gpc-spec/
interface mixin GlobalPrivacyControl {
  [Pref="privacy.globalprivacycontrol.functionality.enabled"]
  readonly attribute boolean globalPrivacyControl;
};

// http://www.w3.org/TR/geolocation-API/#geolocation_interface
interface mixin NavigatorGeolocation {
  [Throws, Pref="geo.enabled"]
  readonly attribute Geolocation geolocation;
};
Navigator includes NavigatorGeolocation;

// http://www.w3.org/TR/battery-status/#navigatorbattery-interface
partial interface Navigator {
  // ChromeOnly to prevent web content from fingerprinting users' batteries.
  [Throws, ChromeOnly, Pref="dom.battery.enabled"]
  Promise<BatteryManager> getBattery();
};

// http://www.w3.org/TR/vibration/#vibration-interface
partial interface Navigator {
    // We don't support sequences in unions yet
    //boolean vibrate ((unsigned long or sequence<unsigned long>) pattern);
    boolean vibrate(unsigned long duration);
    boolean vibrate(sequence<unsigned long> pattern);
};

// http://www.w3.org/TR/pointerevents/#extensions-to-the-navigator-interface
partial interface Navigator {
    [NeedsCallerType]
    readonly attribute long maxTouchPoints;
};

// https://wicg.github.io/media-capabilities/#idl-index
[Exposed=Window]
partial interface Navigator {
  [SameObject, Func="mozilla::dom::MediaCapabilities::Enabled"]
  readonly attribute MediaCapabilities mediaCapabilities;
};

// Mozilla-specific extensions

// Chrome-only interface for Vibration API permission handling.
partial interface Navigator {
    /* Set permission state to device vibration.
     * @param permitted permission state (true for allowing vibration)
     * @param persistent make the permission session-persistent
     */
    [ChromeOnly]
    undefined setVibrationPermission(boolean permitted,
                                     optional boolean persistent = true);
};

partial interface Navigator {
  [Throws, Constant, Cached, NeedsCallerType]
  readonly attribute DOMString oscpu;
  // WebKit/Blink support this; Trident/Presto do not.
  readonly attribute DOMString vendor;
  // WebKit/Blink supports this (hardcoded ""); Trident/Presto do not.
  readonly attribute DOMString vendorSub;
  // WebKit/Blink supports this (hardcoded "20030107"); Trident/Presto don't
  readonly attribute DOMString productSub;
  // WebKit/Blink/Trident/Presto support this.
  readonly attribute boolean cookieEnabled;
  [Throws, Constant, Cached, NeedsCallerType]
  readonly attribute DOMString buildID;

  // WebKit/Blink/Trident/Presto support this.
  [Affects=Nothing, DependsOn=Nothing]
  boolean javaEnabled();
};

// Addon manager bits
partial interface Navigator {
  [Throws, Func="mozilla::AddonManagerWebAPI::IsAPIEnabled"]
  readonly attribute AddonManager mozAddonManager;
};

// NetworkInformation
partial interface Navigator {
  [Throws, Pref="dom.netinfo.enabled"]
  readonly attribute NetworkInformation connection;
};

// https://dvcs.w3.org/hg/gamepad/raw-file/default/gamepad.html#navigator-interface-extension
partial interface Navigator {
  [Throws, Pref="dom.gamepad.enabled", SecureContext]
  sequence<Gamepad?> getGamepads();
};
partial interface Navigator {
  [Throws, Pref="dom.gamepad.test.enabled"]
  GamepadServiceTest requestGamepadServiceTest();
};

// https://immersive-web.github.io/webvr/spec/1.1/#interface-navigator
partial interface Navigator {
  [NewObject, SecureContext, Pref="dom.vr.enabled"]
  Promise<sequence<VRDisplay>> getVRDisplays();
  // TODO: Use FrozenArray once available. (Bug 1236777)
  [SecureContext, Frozen, Cached, Pure, Pref="dom.vr.enabled"]
  readonly attribute sequence<VRDisplay> activeVRDisplays;
  [ChromeOnly, Pref="dom.vr.enabled"]
  readonly attribute boolean isWebVRContentDetected;
  [ChromeOnly, Pref="dom.vr.enabled"]
  readonly attribute boolean isWebVRContentPresenting;
  [ChromeOnly, Pref="dom.vr.enabled"]
  undefined requestVRPresentation(VRDisplay display);
};
partial interface Navigator {
  [Throws, Pref="dom.vr.puppet.enabled"]
  VRServiceTest requestVRServiceTest();
};

// https://immersive-web.github.io/webxr/#dom-navigator-xr
partial interface Navigator {
  [SecureContext, SameObject, Throws, Pref="dom.vr.webxr.enabled"]
  readonly attribute XRSystem xr;
};

// http://webaudio.github.io/web-midi-api/#requestmidiaccess
partial interface Navigator {
  [NewObject, Func="Navigator::HasMidiSupport"]
  Promise<MIDIAccess> requestMIDIAccess(optional MIDIOptions options = {});
};

callback NavigatorUserMediaSuccessCallback = undefined (MediaStream stream);
callback NavigatorUserMediaErrorCallback = undefined (MediaStreamError error);

partial interface Navigator {
  [Throws, Func="Navigator::HasUserMediaSupport"]
  readonly attribute MediaDevices mediaDevices;

  // Deprecated. Use mediaDevices.getUserMedia instead.
  [Deprecated="NavigatorGetUserMedia", Throws,
   Func="Navigator::HasUserMediaSupport",
   NeedsCallerType,
   UseCounter]
  undefined mozGetUserMedia(MediaStreamConstraints constraints,
                            NavigatorUserMediaSuccessCallback successCallback,
                            NavigatorUserMediaErrorCallback errorCallback);
};

// Service Workers/Navigation Controllers
partial interface Navigator {
  [Func="ServiceWorkersEnabled", SameObject, BinaryName="serviceWorkerJS"]
  readonly attribute ServiceWorkerContainer serviceWorker;
};

partial interface Navigator {
  [Throws, Pref="beacon.enabled"]
  boolean sendBeacon(DOMString url,
                     optional BodyInit? data = null);
};

partial interface Navigator {
  [NewObject, Func="mozilla::dom::TCPSocket::ShouldTCPSocketExist"]
  readonly attribute LegacyMozTCPSocket mozTCPSocket;
};

partial interface Navigator {
  [NewObject]
  Promise<MediaKeySystemAccess>
  requestMediaKeySystemAccess(DOMString keySystem,
                              sequence<MediaKeySystemConfiguration> supportedConfigurations);
};

interface mixin NavigatorConcurrentHardware {
  readonly attribute unsigned long long hardwareConcurrency;
};

// https://w3c.github.io/webappsec-credential-management/#framework-credential-management
partial interface Navigator {
  [Pref="security.webauth.webauthn", SecureContext, SameObject]
  readonly attribute CredentialsContainer credentials;
};

// https://w3c.github.io/webdriver/webdriver-spec.html#interface
interface mixin NavigatorAutomationInformation {
  [Constant, Cached]
  readonly attribute boolean webdriver;
};

// https://www.w3.org/TR/clipboard-apis/#navigator-interface
partial interface Navigator {
  [SecureContext, SameObject]
  readonly attribute Clipboard clipboard;
};

// Used for testing of origin trials.
partial interface Navigator {
  [Trial="TestTrial"]
  readonly attribute boolean testTrialGatedAttribute;
};

// https://wicg.github.io/web-share/#navigator-interface
partial interface Navigator {
  [SecureContext, NewObject, Func="Navigator::HasShareSupport"]
  Promise<undefined> share(optional ShareData data = {});
  [SecureContext, Func="Navigator::HasShareSupport"]
  boolean canShare(optional ShareData data = {});
};
// https://wicg.github.io/web-share/#sharedata-dictionary
dictionary ShareData {
  USVString title;
  USVString text;
  USVString url;
  // Note: we don't actually support files yet
  // we have it here for the .canShare() checks.
  sequence<File> files;
};

// https://w3c.github.io/mediasession/#idl-index
[Exposed=Window]
partial interface Navigator {
  [SameObject]
  readonly attribute MediaSession mediaSession;
};

// https://w3c.github.io/web-locks/#navigator-mixins
[SecureContext]
interface mixin NavigatorLocks {
  readonly attribute LockManager locks;
};
Navigator includes NavigatorLocks;

// https://w3c.github.io/autoplay/#autoplay-policy
enum AutoplayPolicy {
  "allowed",
  "allowed-muted",
  "disallowed"
};

enum AutoplayPolicyMediaType {
  "mediaelement",
  "audiocontext"
};

// https://w3c.github.io/autoplay/#autoplay-detection-methods
partial interface Navigator {
  [Pref="dom.media.autoplay-policy-detection.enabled"]
  AutoplayPolicy getAutoplayPolicy(AutoplayPolicyMediaType type);

  [Pref="dom.media.autoplay-policy-detection.enabled"]
  AutoplayPolicy getAutoplayPolicy(HTMLMediaElement element);

  [Pref="dom.media.autoplay-policy-detection.enabled"]
  AutoplayPolicy getAutoplayPolicy(AudioContext context);
};

// https://html.spec.whatwg.org/multipage/interaction.html#the-useractivation-interface
partial interface Navigator {
  [SameObject] readonly attribute UserActivation userActivation;
};

// https://w3c.github.io/screen-wake-lock/#extensions-to-the-navigator-interface
[SecureContext]
partial interface Navigator {
  [SameObject, Pref="dom.screenwakelock.enabled"]
  readonly attribute WakeLock wakeLock;
};
