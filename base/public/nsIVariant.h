/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIVariant_h___
#define nsIVariant_h___

#include "nsISupports.h"
#include "nscore.h"
#include "prtime.h"

enum nsVariantType {
    // primitive values
    nsVariantType_PRBool,
    nsVariantType_PRInt16,
    nsVariantType_PRUint16,
    nsVariantType_PRInt32,
    nsVariantType_PRUint32,
    nsVariantType_PRInt64,
    nsVariantType_PRUint64,
    nsVariantType_float,
    nsVariantType_PRFloat64,
    nsVariantType_PRTime,
    // only pointers after this point -- these will be deleted
    // when the variant is deleted
    nsVariantType_voidPtr,
    nsVariantType_charPtr,
    nsVariantType_PRUnicharPtr
};

class nsVariantValue {
public:
    nsVariantValue() {}

//    nsVariantValue(PRBool value) { mUnion._PRBool = value; }
    nsVariantValue(PRInt16 value) { mUnion._PRInt16 = value; }
    nsVariantValue(PRUint16 value) { mUnion._PRUint16 = value; }
    nsVariantValue(PRInt32 value) { mUnion._PRInt32 = value; }
    nsVariantValue(PRUint32 value) { mUnion._PRUint32 = value; }
    nsVariantValue(PRInt64 value) { mUnion._PRInt64 = value; }
    nsVariantValue(PRUint64 value) { mUnion._PRUint64 = value; }
    nsVariantValue(float value) { mUnion._float = value; }
    nsVariantValue(PRFloat64 value) { mUnion._PRFloat64 = value; }
//    nsVariantValue(PRTime value) { mUnion._PRTime = value; }
    nsVariantValue(void* value) { mUnion._voidPtr = value; }
    nsVariantValue(char* value) { mUnion._charPtr = value; }
    nsVariantValue(PRUnichar* value) { mUnion._PRUnicharPtr = value; }

//    operator PRBool() { return mUnion._PRBool; }
    operator PRInt16() { return mUnion._PRInt16; }
    operator PRUint16() { return mUnion._PRUint16; }
    operator PRInt32() { return mUnion._PRInt32; }
    operator PRUint32() { return mUnion._PRUint32; }
    operator PRInt64() { return mUnion._PRInt64; }
    operator PRUint64() { return mUnion._PRUint64; }
    operator float() { return mUnion._float; }
    operator PRFloat64() { return mUnion._PRFloat64; }
//    operator PRTime() { return mUnion._PRTime; }
    operator void*() { return mUnion._voidPtr; }
    operator const char*() { return mUnion._charPtr; }
    operator const PRUnichar*() { return mUnion._PRUnicharPtr; }

    friend class nsVariant;
protected:
    union nsVariantValueUnion {
        PRBool              _PRBool;
        PRInt16             _PRInt16;
        PRUint16            _PRUint16;
        PRInt32             _PRInt32;
        PRUint32            _PRUint32;
        PRInt64             _PRInt64;
        PRUint64            _PRUint64;
        float               _float;
        PRFloat64           _PRFloat64;
        PRTime              _PRTime;
        void*               _voidPtr;
        char*               _charPtr;
        PRUnichar*          _PRUnicharPtr;
    };

    nsVariantValueUnion mUnion;
};

#define NS_IVARIANT_IID                              \
{ /* 3b5799d0-dc28-11d2-9311-00e09805570f */         \
    0x3b5799d0,                                      \
    0xdc28,                                          \
    0x11d2,                                          \
    {0x93, 0x11, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

class nsIVariant : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IVARIANT_IID; return iid; }
    
    /**
     * Gets the type and value of a Variant. 
     * When the value is a pointer type, the pointer points into the variant's 
     * internal storage. It is the caller's responsibility to copy.
     */
    NS_IMETHOD GetValue(nsVariantType *type, nsVariantValue *value) = 0;

    /**
     * Gets the value of a Variant.
     * @return NS_ERROR_FAILURE if the value is not of the expected type.
     * When the value is a pointer type, the pointer points into the variant's 
     * internal storage. It is the caller's responsibility to copy.
     */
    NS_IMETHOD GetValue(nsVariantType expectedType, nsVariantValue *value) = 0;

    /**
     * Sets the type and value of a Variant.
     * When the value is a pointer type, the variant takes ownership of the
     * pointer. When the variant is released, the pointer will be deleted. 
     */
    NS_IMETHOD SetValue(nsVariantType type, nsVariantValue& value) = 0;

    /**
     * Determines whether two variants have the same internal type and value. 
     * @return NS_OK if they are equal
     * @return NS_COMFALSE if not, or if the other parameter is not an nsIVariant
     */
    NS_IMETHOD Equals(nsISupports* other) = 0;

#ifdef NS_DEBUG
    NS_IMETHOD GetDescription(char* *result) = 0;
#endif

};

extern NS_BASE nsresult
NS_NewIVariant(nsVariantType initialType, nsVariantValue& initialValue,
               nsIVariant* *result);

#endif // nsIVariant_h___
