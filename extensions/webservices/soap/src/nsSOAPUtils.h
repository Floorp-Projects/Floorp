/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsSOAPUtils_h__
#define nsSOAPUtils_h__

#include "nsIDOMElement.h"
#include "nsString.h"
#include "jsapi.h"

class nsSOAPUtils {
public:
  static void GetFirstChildElement(nsIDOMElement* aParent, 
                                   nsIDOMElement** aElement);
  static void GetNextSiblingElement(nsIDOMElement* aStart, 
                                    nsIDOMElement** aElement);
  static void GetElementTextContent(nsIDOMElement* aElement, 
                                    nsString& aText);
  static PRBool HasChildElements(nsIDOMElement* aElement);
  static void GetInheritedEncodingStyle(nsIDOMElement* aEntry, 
                                        char** aEncodingStyle);
  static JSContext* GetSafeContext();
  static JSContext* GetCurrentContext();
  static nsresult ConvertValueToJSVal(JSContext* aContext, 
                                      nsISupports* aValue, 
                                      JSObject* aJSValue, 
                                      PRInt32 aType,
                                      jsval* vp);
  static nsresult ConvertJSValToValue(JSContext* aContext,
                                      jsval val, 
                                      nsISupports** aValue,
                                      JSObject** aJSValue,
                                      PRInt32* aType);

  static const char* kSOAPEnvURI;
  static const char* kSOAPEncodingURI;
  static const char* kSOAPEnvPrefix;
  static const char* kSOAPEncodingPrefix;
  static const char* kXSIURI;
  static const char* kXSDURI;
  static const char* kXSIPrefix;
  static const char* kXSDPrefix;
  static const char* kEncodingStyleAttribute;
  static const char* kEnvelopeTagName;
  static const char* kHeaderTagName;
  static const char* kBodyTagName;
  static const char* kFaultTagName;
  static const char* kFaultCodeTagName;
  static const char* kFaultStringTagName;
  static const char* kFaultActorTagName;
  static const char* kFaultDetailTagName;
};

#endif
