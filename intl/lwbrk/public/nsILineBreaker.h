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
#ifndef nsILineBreaker_h__
#define nsILineBreaker_h__

#include "nsISupports.h"

#include "nscore.h"

#define NS_LINEBREAKER_NEED_MORE_TEXT -1

// {5ae68851-d9a3-49fd-9388-58586dad8044}
#define NS_ILINEBREAKER_IID \
{ 0x5ae68851, 0xd9a3, 0x49fd, \
    { 0x93, 0x88, 0x58, 0x58, 0x6d, 0xad, 0x80, 0x44 } }

class nsILineBreaker : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILINEBREAKER_IID)
  virtual PRInt32 Next( const PRUnichar* aText, PRUint32 aLen, 
                        PRUint32 aPos) = 0;

  virtual PRInt32 Prev( const PRUnichar* aText, PRUint32 aLen, 
                        PRUint32 aPos) = 0;

  // Call this on a word with whitespace at either end. We will apply JISx4501
  // rules to find breaks inside the word. aBreakBefore is set to the break-
  // before status of each character; aBreakBefore[0] will always be false
  // because we never return a break before the first character.
  // aLength is the length of the aText array and also the length of the aBreakBefore
  // output array.
  virtual void GetJISx4051Breaks(const PRUnichar* aText, PRUint32 aLength,
                                 PRPackedBool* aBreakBefore) = 0;
  virtual void GetJISx4051Breaks(const PRUint8* aText, PRUint32 aLength,
                                 PRPackedBool* aBreakBefore) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILineBreaker, NS_ILINEBREAKER_IID)

static inline PRBool
NS_IsSpace(PRUnichar u)
{
  return u == 0x0020 ||                  // SPACE
         u == 0x0009 ||                  // CHARACTER TABULATION
         u == 0x000D ||                  // CARRIAGE RETURN
         (0x2000 <= u && u <= 0x2006) || // EN QUAD, EM QUAD, EN SPACE,
                                         // EM SPACE, THREE-PER-EM SPACE,
                                         // FOUR-PER-SPACE, SIX-PER-EM SPACE,
         (0x2008 <= u && u <= 0x200B) || // PUNCTUATION SPACE, THIN SPACE,
                                         // HAIR SPACE, ZERO WIDTH SPACE
         u == 0x3000;                    // IDEOGRAPHIC SPACE
}

static inline PRBool
NS_NeedsPlatformNativeHandling(PRUnichar aChar)
{
  return (0x0e01 <= aChar && aChar <= 0x0fff); // Thai, Lao, Tibetan
}

#endif  /* nsILineBreaker_h__ */
