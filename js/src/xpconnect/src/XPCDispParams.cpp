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
 * The Original Code is the IDispatch implementation for XPConnect.
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

/**
 * \file XPCDispParams.cpp
 * Implementation of the helper class XPCDispParams
 */

#include "xpcprivate.h"

XPCDispParams::XPCDispParams(PRUint32 args) : 
    mRefBuffer(mStackRefBuffer + sizeof(VARIANT)),
    mDispParamsAllocated(nsnull),
    mRefBufferAllocated(nsnull),
    mPropID(DISPID_PROPERTYPUT)
#ifdef DEBUG
    ,mInserted(PR_FALSE)
#endif
{
    if(args >= DEFAULT_ARG_ARRAY_SIZE)
    {
        mRefBufferAllocated = new char[RefBufferSize(args)];
        mRefBuffer = mRefBufferAllocated + sizeof(VARIANT);
    }
    // Initialize the full buffer that was allocated
    memset(mRefBuffer - sizeof(VARIANT), 0, RefBufferSize(args));
    // Initialize the IDispatch parameters
    mDispParams.cArgs = args;
    if(args == 0)
        mDispParams.rgvarg = nsnull;
    else if (args <= DEFAULT_ARG_ARRAY_SIZE)
        mDispParams.rgvarg = mStackArgs + 1;
    else
    {
        mDispParamsAllocated = new VARIANT[args + 1];
        mDispParams.rgvarg =  mDispParamsAllocated + 1;
    }
    mDispParams.rgdispidNamedArgs = nsnull;
    mDispParams.cNamedArgs = 0;
}


XPCDispParams::~XPCDispParams()
{
    // Cleanup the variants
    for(PRUint32 index = 0; index < mDispParams.cArgs; ++index)
        VariantClear(mDispParams.rgvarg + index);
    // Cleanup if we allocated the variant array. Remember that
    // our buffer may point one element into the allocate buffer
    delete [] mRefBufferAllocated;
    delete [] mDispParamsAllocated;
}

void XPCDispParams::InsertParam(_variant_t & var)
{
#ifdef DEBUG
    NS_ASSERTION(!mInserted, 
                 "XPCDispParams::InsertParam cannot be called more than once");
    mInserted = PR_TRUE;
#endif
    // Bump the pointer back and increment the arg count
    --mDispParams.rgvarg;
    mRefBuffer -= sizeof(VARIANT);
    ++mDispParams.cArgs;
    // Assign the value
    mDispParams.rgvarg[0] = var.Detach();
    // initialize the ref buffer
    memset(mRefBuffer, 0, sizeof(VARIANT));
}
