/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef __NS_IXTFVISUALWRAPPERPRIVATE_H__
#define __NS_IXTFVISUALWRAPPERPRIVATE_H__

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////
// nsIXTFVisualWrapperPrivate: private interface for visual frames

// {B628361B-4466-407F-AD26-58F282DBB6DD}
#define NS_IXTFVISUALWRAPPERPRIVATE_IID \
{ 0xb628361b, 0x4466, 0x407f, { 0xad, 0x26, 0x58, 0xf2, 0x82, 0xdb, 0xb6, 0xdd } }

class nsIXTFVisualWrapperPrivate : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXTFVISUALWRAPPERPRIVATE_IID)

  NS_IMETHOD CreateAnonymousContent(nsPresContext* aPresContext,
                                    nsISupportsArray& aAnonymousItems) = 0;
  virtual void DidLayout() = 0;
  virtual void GetInsertionPoint(nsIDOMElement** insertionPoint) = 0;
  virtual PRBool ApplyDocumentStyleSheets() = 0;
};

#endif // __NS_IXTFVISUALWRAPPERPRIVATE_H__
