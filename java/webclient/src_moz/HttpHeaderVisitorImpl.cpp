/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Ed Burns <edburns@acm.org>
 */

#include "HttpHeaderVisitorImpl.h"

#include <nsAString.h>
#include <nsPromiseFlatString.h>

#include "ns_globals.h" // for prLogModuleInfo
#include "jni_util.h"

NS_IMPL_ISUPPORTS1(HttpHeaderVisitorImpl, nsIHttpHeaderVisitor)

HttpHeaderVisitorImpl::HttpHeaderVisitorImpl(JNIEnv * yourEnv,
					     jobject yourProperties,
					     jobject yourInitContext) :
    mJNIEnv(yourEnv),
    mInitContext(yourInitContext)
{
    // create the inner properties object, into which we'll store our
    // headers
    mProperties = ::util_NewGlobalRef(mJNIEnv,
				      ::util_CreatePropertiesObject(mJNIEnv, mInitContext));
    
    // store it under the key "headers" in the outer properties object
    ::util_StoreIntoPropertiesObject(mJNIEnv, yourProperties, HEADERS_VALUE,
				     mProperties, mInitContext);
}

HttpHeaderVisitorImpl::~HttpHeaderVisitorImpl()
{
    mJNIEnv = nsnull;
    mProperties = nsnull;
    mInitContext = nsnull;
}

NS_IMETHODIMP
HttpHeaderVisitorImpl::VisitHeader(const nsACString &header, 
				   const nsACString &value)
{
    jstring 
	headerName = (jstring) 
	::util_NewGlobalRef(mJNIEnv,
			    ::util_NewStringUTF(mJNIEnv, 
						PromiseFlatCString(header).get())),
	headerValue = (jstring)
	::util_NewGlobalRef(mJNIEnv,
			    ::util_NewStringUTF(mJNIEnv, 
						PromiseFlatCString(value).get()));
    
    ::util_StoreIntoPropertiesObject(mJNIEnv, mProperties, headerName, 
				     headerValue, mInitContext);
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("HttpHeaderVisitorImpl::VisitHeader: name: %s value: %s\n",
	    PromiseFlatCString(header).get(), 
	    PromiseFlatCString(value).get()));
    //    ::util_DeleteLocalRef(mJNIEnv, headerName);
    //    ::util_DeleteLocalRef(mJNIEnv, headerValue);

    return NS_OK;
}
