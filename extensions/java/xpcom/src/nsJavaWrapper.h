/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
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

#ifndef _nsJavaWrapper_h_
#define _nsJavaWrapper_h_

#include "jni.h"
#include "nsISupports.h"


/**
 * Finds the associated Java wraper for the given XPCOM object and IID.  If no
 * such Java wrapper exists, then a new one is created.
 *
 * @param env           Java environment pointer
 * @param aXPCOMObject  XPCOM object for which to find/create Java wrapper
 * @param aIID          desired interface IID for Java wrapper
 * @param aObjectLoader Java wrapper whose class loader we use for finding
 *                      classes; can be null
 * @param aResult       on success, holds reference to Java wrapper
 *
 * @return  NS_OK if succeeded; all other return values are error codes.
 */
nsresult GetNewOrUsedJavaWrapper(JNIEnv* env, nsISupports* aXPCOMObject,
                                 const nsIID& aIID, jobject aObjectLoader,
                                 jobject* aResult);

/**
 * Returns the XPCOM object for which the given Java proxy was created.
 *
 * @param env           pointer to Java context
 * @param aJavaObject   a Java proxy created by CreateJavaProxy()
 * @param aResult       on exit, holds pointer to XPCOM instance
 *
 * @return NS_OK if the XPCOM object was successfully retrieved;
 *         any other value denotes an error condition.
 */
nsresult GetXPCOMInstFromProxy(JNIEnv* env, jobject aJavaObject,
                               void** aResult);

#endif // _nsJavaWrapper_h_
