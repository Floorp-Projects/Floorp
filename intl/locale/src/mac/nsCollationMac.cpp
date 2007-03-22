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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsCollationMac.h"
#include <Resources.h>
#include <TextUtils.h>
#include <Script.h>
#include "prmem.h"
#include "prmon.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsILocaleService.h"
#include "nsLocaleCID.h"
#include "nsIPlatformCharset.h"
#include "nsIMacLocale.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////////////////////////

/* Copy from FE_StrColl(), macfe/utility/locale.cp. */
static short mac_get_script_sort_id(const short scriptcode)
{
	short itl2num;
	ItlbRecord **ItlbRecordHandle;

	/* get itlb of the system script */
	ItlbRecordHandle = (ItlbRecord **) GetResource('itlb', scriptcode);
	
	/* get itl2 number of current system script from itlb if possible
	 * otherwise, try script manager (Script manager won't update 
	 * itl2 number when the change on the fly )
	 */
	if(ItlbRecordHandle != NULL)
	{
		if(*ItlbRecordHandle == NULL)
			LoadResource((Handle)ItlbRecordHandle);
			
		if(*ItlbRecordHandle != NULL)
			itl2num = (*ItlbRecordHandle)->itlbSort;
		else
			itl2num = GetScriptVariable(scriptcode, smScriptSort);
	} else {	/* Use this as fallback */
		itl2num = GetScriptVariable(scriptcode, smScriptSort);
	}
	
	return itl2num;
}

static Handle itl2Handle;

static int mac_sort_tbl_compare(const void* s1, const void* s2)
{
	return CompareText((Ptr) s1, (Ptr) s2, 1, 1, itl2Handle);
}

static int mac_sort_tbl_init(const short scriptcode, unsigned char *mac_sort_tbl)
{
	int i;
	unsigned char sort_tbl[256];
	
	for (i = 0; i < 256; i++)
		sort_tbl[i] = (unsigned char) i;

	/* Get itl2. */
	itl2Handle = GetResource('itl2', mac_get_script_sort_id(scriptcode));
	if (itl2Handle == NULL)
		return -1;
	
	/* qsort */
	PRMonitor* mon = PR_NewMonitor();
	PR_EnterMonitor(mon);
	qsort((void *) sort_tbl, 256, 1, mac_sort_tbl_compare);
	(void) PR_ExitMonitor(mon);
	PR_DestroyMonitor(mon);
	
	/* Put index to the table so we can map character code to sort oder. */
	for (i = 0; i < 256; i++)
		mac_sort_tbl[sort_tbl[i]] = (unsigned char) i;
		
	return 0;
}

inline unsigned char mac_sort_tbl_search(const unsigned char ch, const unsigned char* mac_sort_tbl)
{
	/* Map character code to sort order. */
	return mac_sort_tbl[ch];
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsCollationMac, nsICollation)


nsCollationMac::nsCollationMac() 
{
  mCollation = NULL;
}

nsCollationMac::~nsCollationMac() 
{
  if (mCollation != NULL)
    delete mCollation;
}

nsresult nsCollationMac::Initialize(nsILocale* locale) 
{
  NS_ASSERTION(mCollation == NULL, "Should only be initialized once.");
  nsresult res;
  mCollation = new nsCollation;
  if (mCollation == NULL) {
    NS_ASSERTION(0, "mCollation creation failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // locale -> script code + charset name
  m_scriptcode = smRoman;

  nsAutoString localeStr;

  // get locale string, use app default if no locale specified
  if (locale == nsnull) {
    nsCOMPtr<nsILocaleService> localeService = 
             do_GetService(NS_LOCALESERVICE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsCOMPtr<nsILocale> appLocale;
      res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), 
                                     localeStr);
      }
    }
  }
  else {
    res = locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), 
                              localeStr);
  }

  if (NS_SUCCEEDED(res)) {
    short scriptcode, langcode, regioncode;
    nsCOMPtr <nsIMacLocale> macLocale = do_GetService(NS_MACLOCALE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      if (NS_SUCCEEDED(res = macLocale->GetPlatformLocale(localeStr, &scriptcode, &langcode, &regioncode))) {
        m_scriptcode = scriptcode;
      }
    }

    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsCAutoString mappedCharset;
      res = platformCharset->GetDefaultCharsetForLocale(localeStr, mappedCharset);
      if (NS_SUCCEEDED(res)) {
        res = mCollation->SetCharset(mappedCharset.get());
      }
    }
  }
  NS_ASSERTION(NS_SUCCEEDED(res), "initialization failed, use default values");

  // Initialize a mapping table for the script code.
  if (mac_sort_tbl_init(m_scriptcode, m_mac_sort_tbl) == -1) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
};


nsresult nsCollationMac::CompareString(PRInt32 strength, 
                                       const nsAString& string1, const nsAString& string2, PRInt32* result)
{
  PRUint32 aLength1, aLength2;
  PRUint8 *aKey1 = nsnull, *aKey2 = nsnull;
  nsresult res;

  res = AllocateRawSortKey(strength, string1, &aKey1, &aLength1);
  if (NS_SUCCEEDED(res)) {
    res = AllocateRawSortKey(strength, string2, &aKey2, &aLength2);
    if (NS_SUCCEEDED(res))
      *result = strcmp((const char *)aKey1, (const char *)aKey2); // compare keys
  }

  // delete keys
  PR_FREEIF(aKey1);
  PR_FREEIF(aKey2);

  return res;
}
 

nsresult nsCollationMac::AllocateRawSortKey(PRInt32 strength, 
                                            const nsAString& stringIn, PRUint8** key, PRUint32* outLen)
{
  nsresult res = NS_OK;

  nsAutoString stringNormalized;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringIn, stringNormalized);
  } else {
    stringNormalized = stringIn;
  }

  // convert unicode to charset
  char *str;
  int str_len;

  res = mCollation->UnicodeToChar(stringNormalized, &str);
  if (NS_SUCCEEDED(res) && str != NULL) {
    str_len = strlen(str);
    *key = (PRUint8 *)str;
    *outLen = str_len + 1;
    
    // If no CJK then generate a collation key
    if (smJapanese != m_scriptcode && smKorean != m_scriptcode && 
        smTradChinese != m_scriptcode && smSimpChinese != m_scriptcode) {
      while (*str) {
        *str = (PRUint8) mac_sort_tbl_search((const unsigned char) *str, m_mac_sort_tbl);
        ++str;
      }
    }
    // No CJK support, just copy the row string.
    // ShiftJIS specific, shift hankaku kana in front of zenkaku.
    else if (smJapanese == m_scriptcode) {
      while (*str) {
        if ((unsigned char) *str >= 0xA0 && (unsigned char) *str < 0xE0) {
          *str -= (0xA0 - 0x81);
        }
        else if ((unsigned char) *str >= 0x81 && (unsigned char) *str < 0xA0) {
          *str += (0xE0 - 0xA0);
        } 
        // advance 2 bytes if the API says so and not passing the end of the string
        if (CharacterByteType((Ptr) str, 0, m_scriptcode) == smFirstByte) {
          ++str;
          if (!*str)
            break;
        }
      ++str;
      }
    }
  }

  return NS_OK;
}

nsresult nsCollationMac::CompareRawSortKey(const PRUint8* key1, PRUint32 len1, 
                                           const PRUint8* key2, PRUint32 len2, 
                                           PRInt32* result)
{
  *result = PL_strcmp((const char *)key1, (const char *)key2);
  return NS_OK;
}
