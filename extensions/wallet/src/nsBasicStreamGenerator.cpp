/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsBasicStreamGenerator.h"

const char * nsBasicStreamGenerator::mSignature = "Basic Keyed Stream Generator";

NS_IMPL_ISUPPORTS1(nsBasicStreamGenerator, nsIKeyedStreamGenerator)

nsBasicStreamGenerator::nsBasicStreamGenerator()
    : mLevel(NS_SECURITY_LEVEL), mSalt(0), mPassword()
{
    NS_INIT_ISUPPORTS();
}

nsBasicStreamGenerator::~nsBasicStreamGenerator()
{
}


NS_IMETHODIMP nsBasicStreamGenerator::GetSignature(char **signature)
{
    NS_ENSURE_ARG_POINTER(signature);
    *signature = nsCRT::strdup(mSignature);
    return NS_OK;
}

NS_IMETHODIMP nsBasicStreamGenerator::Setup(PRUint32 salt, nsISupports *consumer)
{
    nsresult rv = NS_OK;
    // Forget everything about previous setup
    mWeakPasswordSink = nsnull;
    // XXX whipe out the password to zero in memory
    mPassword.Truncate(0);

    // Reestablish setup
    if (consumer)
    {
        mWeakPasswordSink = NS_GetWeakReference(consumer, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    mSalt = salt;
    return NS_OK;
}

NS_IMETHODIMP nsBasicStreamGenerator::GetLevel(float *aLevel)
{
    NS_ENSURE_ARG_POINTER(aLevel);
    *aLevel = mLevel;
    return NS_OK;
}

NS_IMETHODIMP nsBasicStreamGenerator::GetByte(PRUint32 offset, PRUint8 *retval)
{
    NS_ENSURE_ARG_POINTER(retval);
    nsresult rv = NS_OK;
    if (mPassword.Length() == 0)
    {
        // First time we need the password. Get it.
        nsCOMPtr<nsIPasswordSink> weakPasswordSink = do_QueryReferent(mWeakPasswordSink);
        if (!weakPasswordSink) return NS_ERROR_FAILURE;
        PRUnichar *aPassword;
        rv = weakPasswordSink->GetPassword(&aPassword);
        if (NS_FAILED(rv)) return rv;
        mPassword = aPassword;
        nsAllocator::Free(aPassword);
    }

    // Get the offset byte from the stream. Our stream is just our password
    // repeating itself infinite times.
    //
    // mPassword being a nsCString, returns a PRUnichar for operator [].
    // Hence convert it to a const char * first before applying op [].
    *retval = ((const char *)mPassword)[offset % mPassword.Length()];

    return rv;
}
