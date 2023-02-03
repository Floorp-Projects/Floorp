/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 *
 * This IDL file is related to interface mixin for the additional globals that should be
 * available in windows and service workers allowed to access the WebExtensions API and
 * the WebExtensions browser API namespace.
 *
 * More info about generating webidl API bindings for WebExtensions API at:
 *
 * https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/webidl_bindings.html
 *
 * A short summary of the special setup used by these WebIDL files (meant to aid
 * webidl peers reviews and sign-offs) is available in the following section:
 *
 * https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/webidl_bindings.html#review-process-on-changes-to-webidl-definitions
 */

// WebExtensions API interface mixin (used to include ExtensionBrowser interface
// in the ServiceWorkerGlobalScope and Window).
[Exposed=(ServiceWorker)]
interface mixin ExtensionGlobalsMixin {
  [Replaceable, SameObject, BinaryName="AcquireExtensionBrowser",
   BindingAlias="chrome", Func="extensions::ExtensionAPIAllowed"]
  readonly attribute ExtensionBrowser browser;
};

[Exposed=(ServiceWorker), LegacyNoInterfaceObject]
interface ExtensionBrowser {
  // A mock API only exposed in tests to unit test the internals
  // meant to be reused by the real WebExtensions API bindings
  // in xpcshell tests.
  [Replaceable, SameObject, BinaryName="GetExtensionMockAPI",
   Func="mozilla::extensions::ExtensionMockAPI::IsAllowed",
   Pref="extensions.webidl-api.expose_mock_interface"]
  readonly attribute ExtensionMockAPI mockExtensionAPI;

  // `browser.alarms` API namespace
  [Replaceable, SameObject, BinaryName="GetExtensionAlarms",
   Func="mozilla::extensions::ExtensionAlarms::IsAllowed"]
  readonly attribute ExtensionAlarms alarms;

  // `browser.browserSettings` API namespace
  [Replaceable, SameObject, BinaryName="GetExtensionBrowserSettings",
   Func="mozilla::extensions::ExtensionBrowserSettings::IsAllowed"]
  readonly attribute ExtensionBrowserSettings browserSettings;

  // `browser.dns` API namespace
  [Replaceable, SameObject, BinaryName="GetExtensionDns",
   Func="mozilla::extensions::ExtensionDns::IsAllowed"]
  readonly attribute ExtensionDns dns;

  // `browser.proxy` API namespace
  [Replaceable, SameObject, BinaryName="GetExtensionProxy",
   Func="mozilla::extensions::ExtensionProxy::IsAllowed"]
  readonly attribute ExtensionProxy proxy;

  // `browser.runtime` API namespace
  [Replaceable, SameObject, BinaryName="GetExtensionRuntime",
   Func="mozilla::extensions::ExtensionRuntime::IsAllowed"]
  readonly attribute ExtensionRuntime runtime;

  // `browser.scripting` API namespace
  [Replaceable, SameObject, BinaryName="GetExtensionScripting",
    Func="mozilla::extensions::ExtensionScripting::IsAllowed"]
  readonly attribute ExtensionScripting scripting;

  // `browser.test` API namespace, available in tests.
  [Replaceable, SameObject, BinaryName="GetExtensionTest",
   Func="mozilla::extensions::ExtensionTest::IsAllowed"]
  readonly attribute ExtensionTest test;
};
