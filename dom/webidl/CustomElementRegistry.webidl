/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://html.spec.whatwg.org/#dom-window-customelements
[Exposed=Window]
interface CustomElementRegistry {
  [CEReactions, Throws, UseCounter]
  void define(DOMString name, CustomElementConstructor constructor,
              optional ElementDefinitionOptions options = {});
  [ChromeOnly, Throws]
  void setElementCreationCallback(DOMString name, CustomElementCreationCallback callback);
  any get(DOMString name);
  [Throws]
  Promise<CustomElementConstructor> whenDefined(DOMString name);
  [CEReactions] void upgrade(Node root);
};

dictionary ElementDefinitionOptions {
  DOMString extends;
};

callback constructor CustomElementConstructor = any ();

[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback CustomElementCreationCallback = void (DOMString name);

[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleConnectedCallback = void();
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleDisconnectedCallback = void();
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleAdoptedCallback = void(Document? oldDocument,
                                         Document? newDocment);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleAttributeChangedCallback = void(DOMString attrName,
                                                  DOMString? oldValue,
                                                  DOMString? newValue,
                                                  DOMString? namespaceURI);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormAssociatedCallback = void(HTMLFormElement? form);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormResetCallback = void();
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleFormDisabledCallback = void(boolean disabled);
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback LifecycleGetCustomInterfaceCallback = object?(any iid);

[GenerateInit]
dictionary LifecycleCallbacks {
  LifecycleConnectedCallback connectedCallback;
  LifecycleDisconnectedCallback disconnectedCallback;
  LifecycleAdoptedCallback adoptedCallback;
  LifecycleAttributeChangedCallback attributeChangedCallback;
  LifecycleFormAssociatedCallback formAssociatedCallback;
  LifecycleFormResetCallback formResetCallback;
  LifecycleFormDisabledCallback formDisabledCallback;
  [ChromeOnly] LifecycleGetCustomInterfaceCallback getCustomInterfaceCallback;
};
