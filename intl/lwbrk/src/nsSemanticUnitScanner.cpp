/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    //  make sure begin, end and _retval is not nullptr here

    // if we reach the end, just return
    if (pos >= length) {
       *begin = pos;
       *end = pos;
       *_retval = false;
       return NS_OK;
    }

    PRUint8 char_class = nsSampleWordBreaker::GetClass(text[pos]);

    // if we are in chinese mode, return one han letter at a time
    // we should not do this if we are in Japanese or Korean mode
    if (kWbClassHanLetter == char_class) {
       *begin = pos;
       *end = pos+1;
       *_retval = true;
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
    *_retval = true;
    return NS_OK;
}

