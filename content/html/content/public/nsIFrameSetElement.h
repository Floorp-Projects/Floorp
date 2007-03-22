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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu>  (Original author)
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

#ifndef nsIFramesetElement_h___
#define nsIFramesetElement_h___

#include "nsISupports.h"

// IID for the nsIFramesetElement interface
#define NS_IFRAMESETELEMENT_IID \
{ 0xeefe0fe5, 0x44ac, 0x4d7f, \
  { 0xa7, 0x51, 0xf4, 0xaa, 0x5f, 0x22, 0xb0, 0xbf } }

/**
 * The nsFramesetUnit enum is used to denote the type of each entry
 * in the row or column spec.
 */
enum nsFramesetUnit {
  eFramesetUnit_Fixed = 0,
  eFramesetUnit_Percent,
  eFramesetUnit_Relative
};

/**
 * The nsFramesetSpec struct is used to hold a single entry in the
 * row or column spec.
 */
struct nsFramesetSpec {
  nsFramesetUnit mUnit;
  nscoord        mValue;
};

/**
 * This interface is used by the nsFramesetFrame to access the parsed
 * values of the "rows" and "cols" attributes
 */
class nsIFrameSetElement : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFRAMESETELEMENT_IID)

  /**
   * GetRowSpec is used to get the "rows" spec.
   * @param out PRInt32 aNumValues The number of row sizes specified.
   * @param out nsFramesetSpec* aSpecs The array of size specifications.
            This is _not_ owned by the caller, but by the nsIFrameSetElement
            implementation.  DO NOT DELETE IT.
   * @exceptions NS_ERROR_OUT_OF_MEMORY
   */
  NS_IMETHOD GetRowSpec(PRInt32 *aNumValues, const nsFramesetSpec** aSpecs) = 0;

  /**
   * GetColSpec is used to get the "cols" spec
   * @param out PRInt32 aNumValues The number of row sizes specified.
   * @param out nsFramesetSpec* aSpecs The array of size specifications.
            This is _not_ owned by the caller, but by the nsIFrameSetElement
            implementation.  DO NOT DELETE IT.
   * @exceptions NS_ERROR_OUT_OF_MEMORY
   */
  NS_IMETHOD GetColSpec(PRInt32 *aNumValues, const nsFramesetSpec** aSpecs) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrameSetElement, NS_IFRAMESETELEMENT_IID)

#endif // nsIFramesetElement_h___
