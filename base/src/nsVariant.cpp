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

#include "nsIVariant.h"
#include "nsCRT.h"
#ifdef NS_DEBUG
#include "prprf.h"
#endif

class nsVariant : public nsIVariant {
public:
    NS_DECL_ISUPPORTS

    // nsIVariant methods:
    NS_IMETHOD GetValue(nsVariantType *type, nsVariantValue *value);
    NS_IMETHOD GetValue(nsVariantType expectedType, nsVariantValue *value);
    NS_IMETHOD SetValue(nsVariantType type, nsVariantValue& value);
    NS_IMETHOD Equals(nsISupports* other);
#ifdef NS_DEBUG
    NS_IMETHOD GetDescription(char* *result);
#endif

    // nsVariant methods:
    nsVariant(nsVariantType type, nsVariantValue& value);
    virtual ~nsVariant();

protected:
    nsVariantType mType;
    nsVariantValue mValue;
};

NS_IMPL_ISUPPORTS(nsVariant, nsIVariant::GetIID());

nsVariant::nsVariant(nsVariantType type, nsVariantValue& value)
    : mType(type), mValue(value)
{
    NS_INIT_REFCNT();
}

nsVariant::~nsVariant()
{
    switch (mType) {
      case nsVariantType_voidPtr:
        delete (void*)mValue;
        break;
      case nsVariantType_charPtr:
        nsCRT::free(mValue.mUnion._charPtr);
        break;
      case nsVariantType_PRUnicharPtr:
        nsCRT::free(mValue.mUnion._PRUnicharPtr);
        break;
      default:
        break;
    }
}

NS_IMETHODIMP
nsVariant::GetValue(nsVariantType *type, nsVariantValue *value)
{
    NS_PRECONDITION(type && value, "no place to put the result");
    *type = mType;
    *value = mValue;
    return NS_OK;
}

NS_IMETHODIMP
nsVariant::GetValue(nsVariantType expectedType, nsVariantValue *value)
{
    NS_PRECONDITION(value, "no place to put the result");
    if (mType != expectedType)
        return NS_ERROR_FAILURE;
    *value = mValue;
    return NS_OK;
}

NS_IMETHODIMP
nsVariant::SetValue(nsVariantType type, nsVariantValue& value)
{
    mType = type;
    mValue = value;
    return NS_OK;
}

NS_IMETHODIMP
nsVariant::Equals(nsISupports* other)
{
    nsIVariant* otherVariant;
    nsresult rv = other->QueryInterface(nsIVariant::GetIID(), (void**)&otherVariant);
    if (NS_FAILED(rv)) return NS_COMFALSE;

    nsVariantType otherType;
    nsVariantValue otherValue;
    rv = otherVariant->GetValue(&otherType, &otherValue);
    if (NS_FAILED(rv)) return rv;
    if (mType != otherType)
        return NS_COMFALSE;

    PRBool eq = PR_FALSE;
    // this is gross, but I think it's the only way to compare unions:
    switch (mType) {
      case nsVariantType_PRBool:
        eq = (PRBool)mValue == (PRBool)otherValue;
        break;
      case nsVariantType_PRInt16:
        eq = (PRInt16)mValue == (PRInt16)otherValue;
        break;
      case nsVariantType_PRUint16:
        eq = (PRUint16)mValue == (PRUint16)otherValue;
        break;
      case nsVariantType_PRInt32:
        eq = (PRInt32)mValue == (PRInt32)otherValue;
        break;
      case nsVariantType_PRUint32:
        eq = (PRUint32)mValue == (PRUint32)otherValue;
        break;
      case nsVariantType_PRInt64:
        eq = LL_EQ((PRInt64)mValue, (PRInt64)otherValue);
        break;
      case nsVariantType_PRUint64:
        eq = LL_EQ((PRUint64)mValue, (PRUint64)otherValue);
        break;
      case nsVariantType_float:
        eq = (float)mValue == (float)otherValue;
        break;
      case nsVariantType_PRFloat64:
        eq = (PRFloat64)mValue == (PRFloat64)otherValue;
        break;
      case nsVariantType_PRTime:
        eq = LL_EQ((PRTime)mValue, (PRTime)otherValue);
        break;
      case nsVariantType_voidPtr:
        eq = (void*)mValue == (void*)otherValue;
        break;
      case nsVariantType_charPtr:
        // I hope this shouldn't be comparing pointers:
        eq = nsCRT::strcmp((const char*)mValue, (const char*)otherValue) == 0;
        break;
      case nsVariantType_PRUnicharPtr:
        // I hope this shouldn't be comparing pointers:
        eq = nsCRT::strcmp((const PRUnichar*)mValue, (const PRUnichar*)otherValue) == 0;
        break;
      default:
        NS_ERROR("unknown variant type");
    }
    return eq ? NS_OK : NS_COMFALSE;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsVariant::GetDescription(char* *result)
{
    char* desc;
    switch (mType) {
      case nsVariantType_PRBool:
        desc = nsCRT::strdup((PRBool)mValue ? "true" : "false");
        break;
      case nsVariantType_PRInt16:
        desc = PR_smprintf("%d", (PRInt16)mValue);
        break;
      case nsVariantType_PRUint16:
        desc = PR_smprintf("%u", (PRUint16)mValue);
        break;
      case nsVariantType_PRInt32:
        desc = PR_smprintf("%l", (PRInt32)mValue);
        break;
      case nsVariantType_PRUint32:
        desc = PR_smprintf("%u", (PRUint32)mValue);
        break;
      case nsVariantType_PRInt64:
        desc = PR_smprintf("%ll", (PRInt64)mValue);
        break;
      case nsVariantType_PRUint64:
        desc = PR_smprintf("%ll", (PRUint64)mValue);
        break;
      case nsVariantType_float:
        desc = PR_smprintf("%g", (float)mValue);
        break;
      case nsVariantType_PRFloat64:
        desc = PR_smprintf("%lg", (PRFloat64)mValue);
        break;
      case nsVariantType_PRTime:
        desc = PR_smprintf("%l", (PRTime)mValue);
        break;
      case nsVariantType_voidPtr:
        desc = PR_smprintf("0x%x", (void*)mValue);
        break;
      case nsVariantType_charPtr:
        desc = PR_smprintf("'%s'", (const char*)mValue);
        break;
      case nsVariantType_PRUnicharPtr:
        desc = PR_smprintf("\"%s\"", (const PRUnichar*)mValue);
        break;
      default:
        desc = PR_smprintf("<Variant 0x%x>", this);
    }
    *result = desc;
    return NS_OK;
}
#endif

////////////////////////////////////////////////////////////////////////////////

NS_BASE nsresult
NS_NewIVariant(nsVariantType initialType, nsVariantValue& initialValue,
               nsIVariant* *result)
{
    nsVariant* v = new nsVariant(initialType, initialValue);
    if (v == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(v);
    *result = v;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
