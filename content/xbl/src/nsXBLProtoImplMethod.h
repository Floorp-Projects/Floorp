/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsXBLProtoImplMethod_h__
#define nsXBLProtoImplMethod_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsXBLProtoImplMember.h"

MOZ_DECL_CTOR_COUNTER(nsXBLParameter);

struct nsXBLParameter {
  nsXBLParameter* mNext;
  char* mName;

  nsXBLParameter(const nsAReadableString& aName) {
    MOZ_COUNT_CTOR(nsXBLParameter);
    mName = ToNewCString(aName);
    mNext = nsnull;
  }

  ~nsXBLParameter() {
    MOZ_COUNT_DTOR(nsXBLParameter);
    nsMemory::Free(mName);
    delete mNext;
  }
};

MOZ_DECL_CTOR_COUNTER(nsXBLUncompiledMethod);

struct nsXBLUncompiledMethod {
  nsXBLParameter* mParameters;
  nsXBLParameter* mLastParameter;
  PRUnichar* mBodyText;

  nsXBLUncompiledMethod() {
    MOZ_COUNT_CTOR(nsXBLUncompiledMethod);
    mBodyText = nsnull;
    mParameters = nsnull;
    mLastParameter = nsnull;
  }

  ~nsXBLUncompiledMethod() {
    MOZ_COUNT_DTOR(nsXBLUncompiledMethod);
    nsMemory::Free(mBodyText);
    delete mParameters;
  }

  PRInt32 GetParameterCount() {
    PRInt32 result = 0;
    for (nsXBLParameter* curr = mParameters; curr; curr=curr->mNext)
      result++;
    return result;
  }

  void AppendBodyText(const nsAReadableString& aText) {
    if (mBodyText) {
      PRUnichar* temp = mBodyText;
      mBodyText = ToNewUnicode(nsDependentString(temp) + aText);
      nsMemory::Free(temp);
    }
    else
      mBodyText = ToNewUnicode(aText);
  }

  void AddParameter(const nsAReadableString& aText) {
    nsXBLParameter* param = new nsXBLParameter(aText);
    if (!param)
      return;
    if (!mParameters)
      mParameters = param;
    else
      mLastParameter->mNext = param;
    mLastParameter = param;
  }
};

class nsXBLProtoImplMethod: public nsXBLProtoImplMember
{
public:
  nsXBLProtoImplMethod(const nsAReadableString& aName);
  virtual ~nsXBLProtoImplMethod();
  virtual void Destroy(PRBool aIsCompiled);

  void AppendBodyText(const nsAReadableString& aBody);
  void AddParameter(const nsAReadableString& aName);

  virtual nsresult InstallMember(nsIScriptContext* aContext, nsIContent* aBoundElement, 
                                 void* aScriptObject, void* aTargetClassObject);
  virtual nsresult CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr, void* aClassObject);

protected:
  union {
    nsXBLUncompiledMethod* mUncompiledMethod; // An object that represents the method before being compiled.
    JSObject * mJSMethodObject;               // The JS object for the method (after compilation)
  };
};

#endif // nsXBLProtoImplMethod_h__
