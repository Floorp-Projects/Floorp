/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIScriptObjectOwner_h__
#define nsIScriptObjectOwner_h__

#include "nscore.h"
#include "nsISupports.h"

typedef struct JSContext         JSContext;
typedef struct JSObject          JSObject;

class nsIScriptContext;

#define NS_ISCRIPTOBJECTOWNER_IID \
{ /* 8f6bca7e-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7e, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} } \

/**
 * Creates a link between the script object and its native implementation
 *<P>
 * Every object that wants to be exposed in a script environment should
 * implement this interface. This interface should guarantee that the same
 * script object is returned in the context of the same script.
 * <P><I>It does have a bit too much java script information now, that
 * should be removed in a short time. Ideally this interface will be
 * language neutral</I>
 */
class nsIScriptObjectOwner : public nsISupports {
public:
  /**
   * Return the script object associated with this object.
   * Create a script object if not present.
   *
   * @param aContext the context the script object has to be created in
   * @param aScriptObject on return will contain the script object
   *
   * @return nsresult NS_OK if the script object is successfully returned
   *
   **/
  virtual nsresult  GetScriptObject(JSContext *aContext, void** aScriptObject) = 0;

  /**
   * Nuke the current script object.
   * Next call to GetScriptObject creates a new script object.
   *
   **/
  virtual nsresult  ResetScriptObject() = 0;
};

class nsIDOMDocument;
extern "C" NS_DOM nsresult NS_NewScriptDocument(JSContext *aContext, 
                                                nsIDOMDocument *aDocument, 
                                                JSObject *aParent, 
                                                JSObject **aJSObject);
class nsIDOMElement;
extern "C" NS_DOM nsresult NS_NewScriptElement(JSContext *aContext, 
                                               nsIDOMElement *aElement, 
                                               JSObject *aParent, 
                                               JSObject **aJSObject);
class nsIDOMText;
extern "C" NS_DOM nsresult NS_NewScriptText(JSContext *aContext, 
                                            nsIDOMText *aText, 
                                            JSObject *aParent, 
                                            JSObject **aJSObject);
class nsIDOMNodeIterator;
extern "C" NS_DOM nsresult NS_NewScriptNodeIterator(JSContext *aContext, 
                                                    nsIDOMNodeIterator *aNodeIterator, 
                                                    JSObject *aParent, 
                                                    JSObject **aJSObject);
class nsIDOMAttribute;
extern "C" NS_DOM nsresult NS_NewScriptAttribute(JSContext *aContext, 
                                                 nsIDOMAttribute *aAttribute, 
                                                 JSObject *aParent, 
                                                 JSObject **aJSObject);
class nsIDOMAttributeList;
extern "C" NS_DOM nsresult NS_NewScriptAttributeList(JSContext *aContext, 
                                                     nsIDOMAttributeList *aAttributeList, 
                                                     JSObject *aParent, 
                                                     JSObject **aJSObject);
#endif // nsIScriptObjectOwner_h__
