/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the IDispatch implementation for XPConnect
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xpcprivate.h"

XPCDispParams::XPCDispParams(PRUint32 args) : 
    mVarBuffer(args <= DEFAULT_ARG_ARRAY_SIZE ? 
        mVarStackBuffer : new char[XPC_VARIANT_BUFFER_SIZE(args)]),
    mPropID(DISPID_PROPERTYPUT)
{
    memset(mVarBuffer, 0, XPC_VARIANT_BUFFER_SIZE(args));
    // Initialize the IDispatch parameters
    mDispParams.cArgs = args;
    mDispParams.rgvarg = args <= DEFAULT_ARG_ARRAY_SIZE ? mStackArgs : new VARIANT[args];
    mDispParams.rgdispidNamedArgs = nsnull;
    mDispParams.cNamedArgs = 0;
}

// This transfers the variants, it does not copy
XPCDispParams::XPCDispParams(XPCDispParams & other) :
    mPropID(DISPID_PROPERTYPUT)
{
    mDispParams.rgdispidNamedArgs = nsnull;
    mDispParams.cNamedArgs = 0;
    mDispParams.cArgs = other.mDispParams.cArgs;
    if(other.mVarBuffer == other.mVarStackBuffer)
    {
        mVarBuffer = mVarStackBuffer;
        memcpy(mVarBuffer, other.mVarBuffer, 
               XPC_VARIANT_BUFFER_SIZE(other.mDispParams.cArgs));
    }
    else
    {
        mVarBuffer = other.mVarBuffer;
        other.mVarBuffer = nsnull;
    }
    if(other.mDispParams.rgvarg == other.mStackArgs)
    {
        mDispParams.rgvarg = mStackArgs;
        memcpy(mDispParams.rgvarg, other.mStackArgs, 
               sizeof(VARIANT) * other.mDispParams.cArgs);
        other.mDispParams.cArgs;
    }
    else
    {
        mDispParams.rgvarg = other.mDispParams.rgvarg;
        other.mDispParams.rgvarg = 0;
    }
    other.mDispParams.cArgs = 0;
}


XPCDispParams::~XPCDispParams()
{
    for(PRUint32 index = 0; index < mDispParams.cArgs; ++index)
        VariantClear(mDispParams.rgvarg + index);
    // Cleanup if we allocated the variant array
    if(mDispParams.rgvarg != mStackArgs)
        delete [] mDispParams.rgvarg;
    if(mVarBuffer != mVarStackBuffer)
        delete [] mVarBuffer;
}

void XPCDispParams::InsertParam(_variant_t & var)
{
    VARIANT* oldArgs =  mDispParams.rgvarg;
    char * oldVarBuffer = mVarBuffer;
    if(mDispParams.cNamedArgs >= DEFAULT_ARG_ARRAY_SIZE)
    {
        if(mDispParams.rgvarg == mStackArgs)
            mDispParams.rgvarg = new VARIANT[mDispParams.cArgs + 1];
        if(mVarBuffer == mVarStackBuffer)
            mVarBuffer = new char [XPC_VARIANT_BUFFER_SIZE(mDispParams.cArgs + 1)];
    }
    // Move the data up, using position save copy
    memmove(mDispParams.rgvarg + 1, oldArgs, sizeof(VARIANT) * mDispParams.cArgs);
    memmove(mVarBuffer + VARIANT_UNION_SIZE,oldVarBuffer, XPC_VARIANT_BUFFER_SIZE(mDispParams.cArgs));
    mDispParams.rgvarg[0] = var.Detach();
    memset(mVarBuffer, 0, VARIANT_UNION_SIZE);
    ++mDispParams.cArgs;
}
