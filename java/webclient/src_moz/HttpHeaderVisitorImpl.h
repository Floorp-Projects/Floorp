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

#include "nsIHttpHeaderVisitor.h"

#include "jni.h"

class HttpHeaderVisitorImpl : public nsIHttpHeaderVisitor
{
public:
   NS_DECL_ISUPPORTS
   NS_DECL_NSIHTTPHEADERVISITOR
 
   HttpHeaderVisitorImpl(JNIEnv *myEnv, jobject myProperties, 
			 jobject myInitContext);
   virtual ~HttpHeaderVisitorImpl();

private:
    JNIEnv *mJNIEnv;
    jobject mProperties;
    jobject mInitContext;
 };
