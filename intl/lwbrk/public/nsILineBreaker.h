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

// {7509772F-770C-44e8-AAFA-8032E5A35370}
#define NS_ILINEBREAKER_IID \
{ 0x7509772f, 0x770c, 0x44e8, \
    { 0xaa, 0xfa, 0x80, 0x32, 0xe5, 0xa3, 0x53, 0x70 } }


class nsILineBreaker : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILINEBREAKER_IID)
  virtual PRBool BreakInBetween( const PRUnichar* aText1 , PRUint32 aTextLen1,
                                 const PRUnichar* aText2 , 
                                 PRUint32 aTextLen2) = 0;

  virtual PRBool CanBreakBetweenLatin1(PRUnichar aChar1,
                                       PRUnichar aChar2) = 0; 


  virtual PRInt32 Next( const PRUnichar* aText, PRUint32 aLen, 
                        PRUint32 aPos) = 0;

  virtual PRInt32 Prev( const PRUnichar* aText, PRUint32 aLen, 
                        PRUint32 aPos) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILineBreaker, NS_ILINEBREAKER_IID)

#endif  /* nsILineBreaker_h__ */
