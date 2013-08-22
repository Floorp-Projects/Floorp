/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAXPCNativeCallContext_h__
#define nsAXPCNativeCallContext_h__

class nsIXPConnectWrappedNative;

/**
* A native call context is allocated on the stack when XPConnect calls a
* native method. Holding a pointer to this object beyond the currently
* executing stack frame is not permitted.
*/
class nsAXPCNativeCallContext
{
public:
    NS_IMETHOD GetCallee(nsISupports **aResult) = 0;
    NS_IMETHOD GetCalleeMethodIndex(uint16_t *aResult) = 0;
    NS_IMETHOD GetCalleeWrapper(nsIXPConnectWrappedNative **aResult) = 0;
    NS_IMETHOD GetJSContext(JSContext **aResult) = 0;
    NS_IMETHOD GetArgc(uint32_t *aResult) = 0;
    NS_IMETHOD GetArgvPtr(JS::Value **aResult) = 0;

    // Methods added since mozilla 0.6....

    NS_IMETHOD GetCalleeInterface(nsIInterfaceInfo **aResult) = 0;
    NS_IMETHOD GetCalleeClassInfo(nsIClassInfo **aResult) = 0;

    NS_IMETHOD GetPreviousCallContext(nsAXPCNativeCallContext **aResult) = 0;

    enum { LANG_UNKNOWN = 0,
           LANG_JS      = 1,
           LANG_NATIVE  = 2 };

    NS_IMETHOD GetLanguage(uint16_t *aResult) = 0;
};

#endif
