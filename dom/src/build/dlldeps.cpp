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
#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMLocation.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIScriptEventListener.h"
#include "nsIDOMCSSStyleSheet.h"
#include "jsurl.h"

// Force references to all of the symbols that we want exported from
// the dll that are located in the .lib files we link with

void XXXDomNeverCalled()
{
  nsJSContext* jcx = new nsJSContext(0);
  NS_NewScriptGlobalObject(0);
  NS_NewScriptNavigator(0, 0, 0, 0);
  NS_NewScriptLocation(0, 0, 0, 0);
  NS_NewScriptHTMLDocument(0, 0, 0, 0);
  NS_NewScriptHTMLCollection(0, 0, 0, 0);
  NS_NewScriptEventListener(0, 0, 0);
  NS_NewScriptHTMLImageElement(0, 0, 0, 0);
  NS_NewScriptHTMLFormElement(0, 0, 0, 0);
  NS_NewScriptHTMLInputElement(0, 0, 0, 0);
  NS_NewScriptCSSStyleSheet(0, 0, 0, 0);
}
