/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __inLayoutUtils_h__
#define __inLayoutUtils_h__

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsIEventStateManager.h"
#include "nsIDOMDocument.h"
#include "nsIBindingManager.h"

class inLayoutUtils
{
public:
  static nsIDOMWindowInternal* GetWindowFor(nsIDOMElement* aElement);
  static nsIDOMWindowInternal* GetWindowFor(nsIDOMDocument* aDoc);
  static nsIPresShell* GetPresShellFor(nsISupports* aThing);
  static nsIFrame* GetFrameFor(nsIDOMElement* aElement, nsIPresShell* aShell);
  static nsIRenderingContext* GetRenderingContextFor(nsIPresShell* aShell);
  static nsIEventStateManager* GetEventStateManagerFor(nsIDOMElement *aElement);
  static nsIBindingManager* GetBindingManagerFor(nsIDOMNode* aNode);
  static nsIDOMDocument* GetSubDocumentFor(nsIDOMNode* aNode);
  static nsIDOMNode* GetContainerFor(nsIDOMDocument* aDoc);
  static PRBool IsDocumentElement(nsIDOMNode* aNode);
  static nsPoint GetClientOrigin(nsIFrame* aFrame);
  static nsRect& GetScreenOrigin(nsIDOMElement* aElement);
  static void AdjustRectForMargins(nsIDOMElement* aElement, nsRect& aRect);
  
};

#endif // __inLayoutUtils_h__
