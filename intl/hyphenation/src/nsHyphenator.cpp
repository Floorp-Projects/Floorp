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

#include "hyphen.h"

nsHyphenator::nsHyphenator(nsIFile *aFile)
  : mDict(nsnull)
{
  nsCString path;
  aFile->GetNativePath(path);
  mDict = hnj_hyphen_load(path.get());
#ifdef DEBUG
  if (mDict) {
    printf("loaded hyphenation patterns from %s\n", path.get());
  }
#endif
  nsresult rv;
  mCategories =
    do_GetService(NS_UNICHARCATEGORY_CONTRACTID, &rv);
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
  for (PRUint32 i = 0; i < aString.Length(); i++) {
    PRUnichar ch = aString[i];

    nsIUGenCategory::nsUGenCategory cat = mCategories->Get(ch);
    if (cat == nsIUGenCategory::kLetter || cat == nsIUGenCategory::kMark) {
      if (!inWord) {
        inWord = PR_TRUE;
        wordStart = i;
      }
      wordLimit = i + 1;
      if (i < aString.Length() - 1) {
        continue;
      }
    }

    if (inWord) {
      NS_ConvertUTF16toUTF8 utf8(aString.BeginReading() + wordStart,
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
        PRUint32 utf16offset = wordStart;
        const char *cp = utf8.BeginReading();
        while (cp < utf8.EndReading()) {
          if (UTF8traits::isASCII(*cp)) { // single-byte utf8 char
            cp++;
            utf16offset++;
          } else if (UTF8traits::is2byte(*cp)) { // 2-byte sequence
            cp += 2;
            utf16offset++;
          } else if (UTF8traits::is3byte(*cp)) { // 3-byte sequence
            cp += 3;
            utf16offset++;
          } else { // must be a 4-byte sequence (no need to check validity,
                   // as this was just created with NS_ConvertUTF16toUTF8)
            NS_ASSERTION(UTF8traits::is4byte(*cp), "unexpected utf8 byte");
            cp += 4;
            utf16offset += 2;
          }
          NS_ASSERTION(cp <= utf8.EndReading(), "incomplete utf8 string?");
          NS_ASSERTION(utf16offset <= aString.Length(), "length mismatch?");
          if (utf8hyphens[cp - utf8.BeginReading() - 1] & 0x01) {
            aHyphens[utf16offset - 1] = PR_TRUE;
          }
        }
      }
    }
    
    inWord = PR_FALSE;
  }

  return NS_OK;
}
