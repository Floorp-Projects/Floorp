/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inLayoutUtils_h__
#define __inLayoutUtils_h__

class nsIDocument;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMNode;
class nsIDOMWindow;
class nsIFrame;
class nsIPresShell;
class nsISupports;

namespace mozilla {
class EventStateManager;
}

class inLayoutUtils
{
public:
  static mozilla::EventStateManager*
           GetEventStateManagerFor(nsIDOMElement *aElement);
  static nsIDOMDocument* GetSubDocumentFor(nsIDOMNode* aNode);
  static nsIDOMNode* GetContainerFor(const nsIDocument& aDoc);
};

#endif // __inLayoutUtils_h__
