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

#ifndef nsXBLProtoImplProperty_h__
#define nsXBLProtoImplProperty_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsXBLProtoImplMember.h"

class nsXBLProtoImplProperty: public nsXBLProtoImplMember
{
public:
  nsXBLProtoImplProperty(const nsAReadableString* aName,
                         const nsAReadableString* aGetter, 
                         const nsAReadableString* aSetter,
                         const nsAReadableString* aReadOnly);
 
  virtual ~nsXBLProtoImplProperty();
  virtual void Destroy(PRBool aIsCompiled);

  void AppendGetterText(const nsAReadableString& aGetter);
  void AppendSetterText(const nsAReadableString& aSetter);

  virtual nsresult InstallMember(nsIScriptContext* aContext, nsIContent* aBoundElement, 
                                 void* aScriptObject, void* aTargetClassObject);
  virtual nsresult CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr, void* aClassObject);

protected:
  union {
    PRUnichar* mGetterText;     // The raw text for the getter (prior to compilation).
    JSObject * mJSGetterObject; // The JS object for the getter (after compilation)
  };

  union {
    PRUnichar* mSetterText;     // The raw text for the setter (prior to compilation).
    JSObject * mJSSetterObject; // The JS object for the setter (after compilation)
  };
  
  uintN mJSAttributes;          // A flag for all our JS properties (getter/setter/readonly/shared/enum)
  PRBool mCompiled;             // Whether or not we are compiled.
};

#endif // nsXBLProtoImplProperty_h__
