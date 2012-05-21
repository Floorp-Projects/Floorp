/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inLayoutUtils_h__
#define __inLayoutUtils_h__

class nsBindingManager;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMNode;
class nsIDOMWindow;
class nsEventStateManager;
class nsIFrame;
class nsIPresShell;
class nsISupports;

class inLayoutUtils
{
public:
  static nsIDOMWindow* GetWindowFor(nsIDOMNode* aNode);
  static nsIDOMWindow* GetWindowFor(nsIDOMDocument* aDoc);
  static nsIPresShell* GetPresShellFor(nsISupports* aThing);
  static nsIFrame* GetFrameFor(nsIDOMElement* aElement);
  static nsEventStateManager* GetEventStateManagerFor(nsIDOMElement *aElement);
  static nsBindingManager* GetBindingManagerFor(nsIDOMNode* aNode);
  static nsIDOMDocument* GetSubDocumentFor(nsIDOMNode* aNode);
  static nsIDOMNode* GetContainerFor(nsIDOMDocument* aDoc);
};

#endif // __inLayoutUtils_h__
