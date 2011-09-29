/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* container for a document and its presentation */

#ifndef nsIDocumentViewer_h___
#define nsIDocumentViewer_h___

#include "nsIContentViewer.h"

class nsIDocument;
class nsPresContext;
class nsIPresShell;
class nsIStyleSheet;
class nsIView;

class nsDOMNavigationTiming;

#define NS_IDOCUMENT_VIEWER_IID \
  { 0x5a5c9a1d, 0x49c4, 0x4f3f, \
    { 0x80, 0xcd, 0x12, 0x09, 0x5b, 0x1e, 0x1f, 0x61 } }

/**
 * A document viewer is a kind of content viewer that uses NGLayout
 * to manage the presentation of the content.
 */
class nsIDocumentViewer : public nsIContentViewer
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_VIEWER_IID)
  
  NS_IMETHOD GetPresShell(nsIPresShell** aResult) = 0;
  
  NS_IMETHOD GetPresContext(nsPresContext** aResult) = 0;

  NS_IMETHOD SetDocumentInternal(nsIDocument* aDocument,
                                 bool aForceReuseInnerWindow) = 0;

  virtual nsIView* FindContainerView() = 0;

  virtual void SetNavigationTiming(nsDOMNavigationTiming* timing) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentViewer, NS_IDOCUMENT_VIEWER_IID)

#endif /* nsIDocumentViewer_h___ */
