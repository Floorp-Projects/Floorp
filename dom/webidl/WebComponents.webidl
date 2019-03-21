/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dvcs.w3.org/hg/webcomponents/raw-file/tip/spec/custom/index.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

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
callback LifecycleGetCustomInterfaceCallback = object?(any iid);

dictionary LifecycleCallbacks {
  LifecycleConnectedCallback connectedCallback;
  LifecycleDisconnectedCallback disconnectedCallback;
  LifecycleAdoptedCallback adoptedCallback;
  LifecycleAttributeChangedCallback attributeChangedCallback;
  [ChromeOnly] LifecycleGetCustomInterfaceCallback getCustomInterfaceCallback;
};
