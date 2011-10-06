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
 * The Original Code is Mozilla Hyphenation Service.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHyphenator.h"
#include "nsIFile.h"
#include "nsUTF8Utils.h"
#include "nsIUGenCategory.h"
#include "nsUnicharUtilCIID.h"
#include "nsNetUtil.h"

#include "hyphen.h"

nsHyphenator::nsHyphenator(nsIFile *aFile)
  : mDict(nsnull)
{
  nsCString urlSpec;
  nsresult rv = NS_GetURLSpecFromFile(aFile, urlSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  mDict = hnj_hyphen_load(urlSpec.get());
#ifdef DEBUG
  if (mDict) {
    printf("loaded hyphenation patterns from %s\n", urlSpec.get());
  }
#endif
  mCategories = do_GetService(NS_UNICHARCATEGORY_CONTRACTID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get category service");
}

nsHyphenator::~nsHyphenator()
{
  if (mDict != nsnull) {
    hnj_hyphen_free((HyphenDict*)mDict);
    mDict = nsnull;
  }
}

PRBool
nsHyphenator::IsValid()
{
  return (mDict != nsnull) && (mCategories != nsnull);
}

nsresult
nsHyphenator::Hyphenate(const nsAString& aString,
                        nsTArray<PRPackedBool>& aHyphens)
{
  if (!aHyphens.SetLength(aString.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(aHyphens.Elements(), PR_FALSE, aHyphens.Length());

  PRBool inWord = PR_FALSE;
  PRUint32 wordStart = 0, wordLimit = 0;
  PRUint32 chLen;
  for (PRUint32 i = 0; i < aString.Length(); i += chLen) {
    PRUint32 ch = aString[i];
    chLen = 1;

    if (NS_IS_HIGH_SURROGATE(ch)) {
      if (i + 1 < aString.Length() && NS_IS_LOW_SURROGATE(aString[i+1])) {
        ch = SURROGATE_TO_UCS4(ch, aString[i+1]);
        chLen = 2;
      } else {
        NS_WARNING("unpaired surrogate found during hyphenation");
      }
    }

    nsIUGenCategory::nsUGenCategory cat = mCategories->Get(ch);
    if (cat == nsIUGenCategory::kLetter || cat == nsIUGenCategory::kMark) {
      if (!inWord) {
        inWord = PR_TRUE;
        wordStart = i;
      }
      wordLimit = i + chLen;
      if (i + chLen < aString.Length()) {
        continue;
      }
    }

    if (inWord) {
      const PRUnichar *begin = aString.BeginReading();
      NS_ConvertUTF16toUTF8 utf8(begin + wordStart,
                                 wordLimit - wordStart);
      nsAutoTArray<char,200> utf8hyphens;
      utf8hyphens.SetLength(utf8.Length() + 5);
      char **rep = nsnull;
      int *pos = nsnull;
      int *cut = nsnull;
      int err = hnj_hyphen_hyphenate2((HyphenDict*)mDict,
                                      utf8.BeginReading(), utf8.Length(),
                                      utf8hyphens.Elements(), nsnull,
                                      &rep, &pos, &cut);
      if (!err) {
        // Surprisingly, hnj_hyphen_hyphenate2 converts the 'hyphens' buffer
        // from utf8 code unit indexing (which would match the utf8 input
        // string directly) to Unicode character indexing.
        // We then need to convert this to utf16 code unit offsets for Gecko.
        const char *hyphPtr = utf8hyphens.Elements();
        const PRUnichar *cur = begin + wordStart;
        const PRUnichar *end = begin + wordLimit;
        while (cur < end) {
          if (*hyphPtr & 0x01) {
            aHyphens[cur - begin] = PR_TRUE;
          }
          cur++;
          if (cur < end && NS_IS_LOW_SURROGATE(*cur) &&
              NS_IS_HIGH_SURROGATE(*(cur-1)))
          {
            cur++;
          }
          hyphPtr++;
        }
      }
    }
    
    inWord = PR_FALSE;
  }

  return NS_OK;
}
