/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is mozilla.org code
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *
 */

#include "jsdHandles.h"
#include "nsString.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdContext, jsdIContext); 

NS_IMETHODIMP
jsdContext::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdScript, jsdIScript); 

NS_IMETHODIMP
jsdScript::GetJSDScript(JSDScript **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mScript;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetIsActive(PRBool *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_IsActiveScript(mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFileName(PRUnichar **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = nsCString(JSD_GetScriptFilename(mCx, mScript)).ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionName(PRUnichar **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = nsCString(JSD_GetScriptFunctionName(mCx, mScript)).ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetBaseLineNumber(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetScriptBaseLineNumber(mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetLineExtent(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetScriptLineExtent(mCx, mScript);
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdThreadState, jsdIThreadState); 

NS_IMETHODIMP
jsdThreadState::GetJSDThreadState(JSDThreadState **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mThreadState;
    return NS_OK;
}
