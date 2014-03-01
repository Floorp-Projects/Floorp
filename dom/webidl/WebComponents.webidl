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

callback LifecycleCreatedCallback = void();
callback LifecycleAttachedCallback = void();
callback LifecycleDetachedCallback = void();
callback LifecycleAttributeChangedCallback = void(DOMString attrName, DOMString? oldValue, DOMString? newValue);

dictionary LifecycleCallbacks {
  LifecycleCreatedCallback? createdCallback;
  LifecycleAttachedCallback? attachedCallback;
  LifecycleDetachedCallback? detachedCallback;
  LifecycleAttributeChangedCallback? attributeChangedCallback;
};

dictionary ElementRegistrationOptions {
  object? prototype = null;
  DOMString? extends = null;
};
