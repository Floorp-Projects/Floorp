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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsRootAccessibleWrap.h"

#include "Compatibility.h"
#include "nsWinUtils.h"

#include "nsIDOMEventTarget.h"
#include "nsEventListenerManager.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

nsRootAccessibleWrap::
  nsRootAccessibleWrap(nsIDocument* aDocument, nsIContent* aRootContent,
                       nsIPresShell* aPresShell) :
  nsRootAccessible(aDocument, aRootContent, aPresShell)
{
}

nsRootAccessibleWrap::~nsRootAccessibleWrap()
{
}

////////////////////////////////////////////////////////////////////////////////
// nsRootAccessible

void
nsRootAccessibleWrap::DocumentActivated(nsDocAccessible* aDocument)
{
  if (Compatibility::IsDolphin() &&
      nsCoreUtils::IsTabDocument(aDocument->GetDocumentNode())) {
    PRUint32 count = mChildDocuments.Length();
    for (PRUint32 idx = 0; idx < count; idx++) {
      nsDocAccessible* childDoc = mChildDocuments[idx];
      HWND childDocHWND = static_cast<HWND>(childDoc->GetNativeWindow());
      if (childDoc != aDocument)
        nsWinUtils::HideNativeWindow(childDocHWND);
      else
        nsWinUtils::ShowNativeWindow(childDocHWND);
    }
  }
}
