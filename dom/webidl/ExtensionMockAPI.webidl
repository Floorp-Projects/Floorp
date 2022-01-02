/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

// WebIDL definition for the "mockExtensionAPI" WebExtensions API,
// only available in tests and locked behind an about:config preference
// ("extensions.webidl-api.expose_mock_interface").
[Exposed=(ServiceWorker), LegacyNoInterfaceObject]
interface ExtensionMockAPI {
  // Test API methods scenarios.

  [Throws, WebExtensionStub]
  any methodSyncWithReturn(any... args);

  [Throws, WebExtensionStub="NoReturn"]
  void methodNoReturn(any... args);

  [Throws, WebExtensionStub="Async"]
  any methodAsync(any arg0, optional Function cb);

  [Throws, WebExtensionStub="AsyncAmbiguous"]
  any methodAmbiguousArgsAsync(any... args);

  [Throws, WebExtensionStub="ReturnsPort"]
  ExtensionPort methodReturnsPort(DOMString testName);

  // Test API properties.

  [Replaceable]
  readonly attribute any propertyAsErrorObject;

  [Replaceable]
  readonly attribute DOMString propertyAsString;

  // Test API events.

  [Replaceable, SameObject]
  readonly attribute ExtensionEventManager onTestEvent;
};
