/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* A namespace class for static content utilities. */

#ifndef nsContentUtils_h___
#define nsContentUtils_h___

#include "jspubtd.h"
#include "nsAReadableString.h"
#include "nsIDOMScriptObjectFactory.h"

class nsIScriptContext;
class nsIScriptGlobalObject;
class nsIXPConnect;
class nsIContent;
class nsIDocument;


class nsContentUtils
{
public:
  static nsresult Init();

  static nsresult ReparentContentWrapper(nsIContent *aContent,
                                         nsIContent *aNewParent,
                                         nsIDocument *aNewDocument,
                                         nsIDocument *aOldDocument);

  // These are copied from nsJSUtils.h

  static nsresult GetStaticScriptGlobal(JSContext* aContext,
                                        JSObject* aObj,
                                        nsIScriptGlobalObject** aNativeGlobal);

  static nsresult GetStaticScriptContext(JSContext* aContext,
                                         JSObject* aObj,
                                         nsIScriptContext** aScriptContext);

  static nsresult GetDynamicScriptGlobal(JSContext *aContext,
                                         nsIScriptGlobalObject** aNativeGlobal);

  static nsresult GetDynamicScriptContext(JSContext *aContext,
                                          nsIScriptContext** aScriptContext);

  static PRUint32 CopyNewlineNormalizedUnicodeTo(const nsAReadableString& aSource, 
                                                 PRUint32 aSrcOffset, 
                                                 PRUnichar* aDest, 
                                                 PRUint32 aLength,
                                                 PRBool& aLastCharCR);

  static PRUint32 CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAWritableString& aDest);

  static nsISupports *
  GetClassInfoInstance(nsDOMClassInfoID aID);

  static void Shutdown();

private:
  static nsresult doReparentContentWrapper(nsIContent *aChild,
                                           nsIDocument *aNewDocument,
                                           nsIDocument *aOldDocument,
                                           JSContext *cx,
                                           JSObject *parent_obj);


  static nsIDOMScriptObjectFactory *sDOMScriptObjectFactory;

  static nsIXPConnect *sXPConnect;
};

#define NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(_class)                      \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    foundInterface =                                                          \
      nsContentUtils::GetClassInfoInstance(eDOMClassInfo_##_class##_id);      \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  } else

#endif /* nsContentUtils_h___ */
