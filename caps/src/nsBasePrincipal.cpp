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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsBasePrincipal.h"
#include "nsString.h"
#include "plstr.h"

//////////////////////////


nsBasePrincipal::nsBasePrincipal()
    : mCapabilities(nsnull)
{
}

PR_STATIC_CALLBACK(PRBool)
deleteElement(void* aElement, void *aData)
{
    nsHashtable *ht = (nsHashtable *) aElement;
    delete ht;
    return PR_TRUE;
}

nsBasePrincipal::~nsBasePrincipal(void)
{
    mAnnotations.EnumerateForwards(deleteElement, nsnull);
    delete mCapabilities;
}

NS_IMETHODIMP
nsBasePrincipal::GetJSPrincipals(JSPrincipals **jsprin)
{
    if (mJSPrincipals.nsIPrincipalPtr == nsnull) {
        mJSPrincipals.nsIPrincipalPtr = this;
        // No need for a ADDREF since it is a self-reference
    }
    *jsprin = &mJSPrincipals;
    JSPRINCIPALS_HOLD(cx, *jsprin);
    return NS_OK;
}

NS_IMETHODIMP 
nsBasePrincipal::CanEnableCapability(const char *capability, PRInt16 *result)
{
    if (!mCapabilities) {
        *result = nsIPrincipal::ENABLE_UNKNOWN;
        return NS_OK;
    }
    nsStringKey key(capability);
    *result = (PRInt16)(PRInt32)mCapabilities->Get(&key);
    if (!*result)
        *result = nsIPrincipal::ENABLE_UNKNOWN;
    return NS_OK;
}

NS_IMETHODIMP 
nsBasePrincipal::SetCanEnableCapability(const char *capability, 
                                        PRInt16 canEnable)
{
    if (!mCapabilities) {
        mCapabilities = new nsHashtable(7);
        if (!mCapabilities)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    nsStringKey key(capability);
    mCapabilities->Put(&key, (void *) canEnable);
    return NS_OK;
}

NS_IMETHODIMP 
nsBasePrincipal::IsCapabilityEnabled(const char *capability, void *annotation, 
                                     PRBool *result)
{
    *result = PR_FALSE;
    nsHashtable *ht = (nsHashtable *) annotation;
    if (ht) {
        nsStringKey key(capability);
        *result = (ht->Get(&key) == (void *) AnnotationEnabled);
    } 
    return NS_OK;
}

NS_IMETHODIMP 
nsBasePrincipal::EnableCapability(const char *capability, void **annotation) 
{
    return SetCapability(capability, annotation, AnnotationEnabled);
}

NS_IMETHODIMP 
nsBasePrincipal::DisableCapability(const char *capability, void **annotation) 
{
    return SetCapability(capability, annotation, AnnotationDisabled);
}

NS_IMETHODIMP 
nsBasePrincipal::RevertCapability(const char *capability, void **annotation) 
{
    if (*annotation) {
        nsHashtable *ht = (nsHashtable *) *annotation;
        nsStringKey key(capability);
        ht->Remove(&key);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::SetCapability(const char *capability, void **annotation,
                               AnnotationValue value) 
{
    if (*annotation == nsnull) {
        *annotation = new nsHashtable(5);
        if (!*annotation)
            return NS_ERROR_OUT_OF_MEMORY;
        // This object owns its annotations. Save them so we can release
        // them when we destroy this object.
        mAnnotations.AppendElement(*annotation);
    }
    nsHashtable *ht = (nsHashtable *) *annotation;
    nsStringKey key(capability);
    ht->Put(&key, (void *) value);
    return NS_OK;
}

nsresult
nsBasePrincipal::Init(const char* data)
{
    // Parses capabilities strings of the form 
    // "Capability=value ..."
    // ie. "UniversalBrowserRead=0 UniversalBrowserWrite=1"
    // where value is from 0 to 3 as defined in nsIPrincipal.idl
    if (mCapabilities)
        mCapabilities->Reset();
    
    for (;;)
    {
        char* wordEnd = PL_strchr(data, '=');
        if (wordEnd == nsnull)
            break;
        *wordEnd = '\0';
        const char* cap = data;
        data = wordEnd+1; // data is now pointing at the numeric value
        PRInt16 value = (PRInt16)(*data) - (PRInt16)'0';
        nsresult rv = SetCanEnableCapability(cap, value);
        if (NS_FAILED(rv)) return rv;
        if (data[1] == '\0') // End of the data
            break;
        else
            data += 2; // data is now at the beginning of the next capability string
    }
    return NS_OK;
}

PR_STATIC_CALLBACK(PRBool)
AppendCapability(nsHashKey *aKey, void* aData, void* aStr)
{
    nsAutoString name( ((nsStringKey*)aKey)->GetString() );
    char value = (char)((unsigned int)aData) + '0';
    nsString* capStr = (nsString*)aStr;
    
    capStr->Append(' ');
    capStr->Append(name);
    capStr->Append('=');
    capStr->Append(value);
    return (capStr != nsnull);
}   
        
NS_IMETHODIMP
nsBasePrincipal::CapabilitiesToString(char** aStr)
{
    if (!mCapabilities || !aStr)
        return NS_OK;

    nsAutoString capStr;
    // The following line is a guess at how long the capabilities string
    // will be (~15 chars per capability). This will minimize copying.
    capStr.SetCapacity(mCapabilities->Count() * 15);
    mCapabilities->Enumerate(AppendCapability, (void*)&capStr);
    *aStr = capStr.ToNewCString();
    if (!(*aStr)) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}
