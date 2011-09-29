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

#include "nsSemanticUnitScanner.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS1(nsSemanticUnitScanner, nsISemanticUnitScanner)

nsSemanticUnitScanner::nsSemanticUnitScanner() : nsSampleWordBreaker()
{
  /* member initializers and constructor code */
}

nsSemanticUnitScanner::~nsSemanticUnitScanner()
{
  /* destructor code */
}


/* void start (in string characterSet); */
NS_IMETHODIMP nsSemanticUnitScanner::Start(const char *characterSet)
{
    // do nothing for now.
    return NS_OK;
}

/* void next (in wstring text, in long length, in long pos, out boolean hasMoreUnits, out long begin, out long end); */
NS_IMETHODIMP nsSemanticUnitScanner::Next(const PRUnichar *text, PRInt32 length, PRInt32 pos, bool isLastBuffer, PRInt32 *begin, PRInt32 *end, bool *_retval)
{
    // xxx need to bullet proff and check input pointer 
    //  make sure begin, end and _retval is not nsnull here

    // if we reach the end, just return
    if (pos >= length) {
       *begin = pos;
       *end = pos;
       *_retval = PR_FALSE;
       return NS_OK;
    }

    PRUint8 char_class = nsSampleWordBreaker::GetClass(text[pos]);

    // if we are in chinese mode, return one han letter at a time
    // we should not do this if we are in Japanese or Korean mode
    if (kWbClassHanLetter == char_class) {
       *begin = pos;
       *end = pos+1;
       *_retval = PR_TRUE;
       return NS_OK;
    }

    PRInt32 next;
    // find the next "word"
    next = NextWord(text, (PRUint32) length, (PRUint32) pos);

    // if we don't have enough text to make decision, return 
    if (next == NS_WORDBREAKER_NEED_MORE_TEXT) {
       *begin = pos;
       *end = isLastBuffer ? length : pos;
       *_retval = isLastBuffer;
       return NS_OK;
    } 
    
    // if what we got is space or punct, look at the next break
    if ((char_class == kWbClassSpace) || (char_class == kWbClassPunct)) {
        // if the next "word" is not letters, 
        // call itself recursively with the new pos
        return Next(text, length, next, isLastBuffer, begin, end, _retval);
    }

    // for the rest, return 
    *begin = pos;
    *end = next;
    *_retval = PR_TRUE;
    return NS_OK;
}

