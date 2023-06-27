/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://html.spec.whatwg.org/#dom-window-customelements
[Exposed=Window]
interface CustomElementRegistry {
  [CEReactions, Throws, UseCounter]
  undefined define(DOMString name, CustomElementConstructor constructor,
                   optional ElementDefinitionOptions options = {});
  [ChromeOnly, Throws]
  undefined setElementCreationCallback(DOMString name, CustomElementCreationCallback callback);
  (CustomElementConstructor or undefined) get(DOMString name);
  DOMString? getName(CustomElementConstructor constructor);
  [Throws]
  Promise<CustomElementConstructor> whenDefined(DOMString name);
  [CEReactions] undefined upgrade(Node root);
};

dictionary ElementDefinitionOptions {
  DOMString extends;
};

enum RestoreReason {
  "restore",
  "autocomplete",
};

callback constructor CustomElementConstructor = any ();

[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback CustomElementCreationCallback = undefined (DOMString name);

[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleConnectedCallback = undefined();
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleDisconnectedCallback = undefined();
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleAdoptedCallback = undefined(Document? oldDocument,
                                              Document? newDocment);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleAttributeChangedCallback = undefined(DOMString attrName,
                                                       DOMString? oldValue,
                                                       DOMString? newValue,
                                                       DOMString? namespaceURI);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormAssociatedCallback = undefined(HTMLFormElement? form);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormResetCallback = undefined();
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormDisabledCallback = undefined(boolean disabled);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormStateRestoreCallback = undefined((File or USVString or FormData)? state, RestoreReason reason);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleGetCustomInterfaceCallback = object?(any iid);

// Unsorted is necessary until https://github.com/whatwg/html/issues/3580 is resolved.
[GenerateInit, Unsorted]
dictionary LifecycleCallbacks {
  LifecycleConnectedCallback connectedCallback;
  LifecycleDisconnectedCallback disconnectedCallback;
  LifecycleAdoptedCallback adoptedCallback;
  LifecycleAttributeChangedCallback attributeChangedCallback;
  [ChromeOnly] LifecycleGetCustomInterfaceCallback getCustomInterfaceCallback;
};

[GenerateInit, Unsorted]
dictionary FormAssociatedLifecycleCallbacks {
  LifecycleFormAssociatedCallback formAssociatedCallback;
  LifecycleFormResetCallback formResetCallback;
  LifecycleFormDisabledCallback formDisabledCallback;
  LifecycleFormStateRestoreCallback formStateRestoreCallback;
};
