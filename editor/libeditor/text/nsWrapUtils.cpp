/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsWrapUtils.h"

#include "nsCOMPtr.h"

// Line breaker stuff
#include "nsIServiceManager.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"

static NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);

/*
 * Rewrap the given section of string, putting the result in aOutString.
 */
nsresult
nsWrapUtils::Rewrap(const nsAReadableString& aInString,
                         PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                         PRBool aRespectNewlines,
                         const nsAReadableString &aLineStartStr,
                         nsAWritableString& aOutString)
{
  PRInt32 i;

  nsCOMPtr<nsILineBreaker> lineBreaker;

  nsILineBreakerFactory *lf;
  nsresult rv = NS_OK;
  rv = nsServiceManager::GetService(kLWBrkCID,
                                    NS_GET_IID(nsILineBreakerFactory),
                                    (nsISupports **)&lf);
  if (NS_SUCCEEDED(rv))
  {
    nsAutoString lbarg;
    rv = lf->GetBreaker(lbarg, getter_AddRefs(lineBreaker));
    nsServiceManager::ReleaseService(kLWBrkCID, lf);
  }

  aOutString.SetLength(0);

  // Now we either have a line breaker, or we don't.
  PRInt32 length = aInString.Length();
  nsString tString(aInString);
  const PRUnichar* unicodeStr = tString.get();
  for (i = 0; i < length; )    // loop over lines
  {
    nsAutoString remaining(unicodeStr + i, length - i);

    // If there's a first-line offset, that means we're not starting
    // at the beginning of the line, so don't add a cite string there:
    if (!aFirstLineOffset)
      aOutString.Append(aLineStartStr);

    PRInt32 eol = i + aWrapCol - aFirstLineOffset;
    if (eol > length)
    {
      aOutString.Append(unicodeStr + i, length - i);
      aOutString.Append(PRUnichar('\n'));  // DOM line breaks, not NS_LINEBREAK
      break;
    }
    if (i > 0) aFirstLineOffset = 0;
    // eol is the prospective end of line ...
    // look backwards from there for a place to break.
    PRUint32 breakPt;
    PRBool needMore;
    rv = NS_ERROR_BASE;
    if (lineBreaker)
    {
      rv = lineBreaker->Prev(unicodeStr + i, length - i, eol - i,
                             &breakPt, &needMore);
      if (NS_FAILED(rv) || needMore)
      {
        rv = lineBreaker->Next(unicodeStr + i, length - i, eol - i,
                               &breakPt, &needMore);
        if (needMore) rv = NS_ERROR_BASE;
      }
    }
    // If we get out here and rv is set, something went wrong with line breaker.
    // Just break the line, hard.
    // If rv is okay, then breakPt is the place to break.
    if (NS_FAILED(rv))
    {
#ifdef DEBUG_akkana
      printf("nsILineBreaker not working -- breaking hard\n");
#endif
      breakPt = eol+1;
    }
    else breakPt += i;
    nsAutoString appending(unicodeStr + i, breakPt - i);
    aOutString.Append(unicodeStr + i, breakPt - i);
    aOutString.Append(PRUnichar('\n'));  // DOM line breaks, not NS_LINEBREAK

    i = breakPt;
  } // continue looping over lines

  return NS_OK;
}

