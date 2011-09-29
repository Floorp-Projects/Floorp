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

#include "nsHyphenationManager.h"
#include "nsHyphenator.h"
#include "nsIAtom.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsDirectoryServiceDefs.h"
#include "nsUnicharUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

#define INTL_HYPHENATIONALIAS_PREFIX "intl.hyphenation-alias."

nsHyphenationManager *nsHyphenationManager::sInstance = nsnull;

nsHyphenationManager*
nsHyphenationManager::Instance()
{
  if (sInstance == nsnull) {
    sInstance = new nsHyphenationManager();
  }
  return sInstance;
}

void
nsHyphenationManager::Shutdown()
{
  delete sInstance;
}

nsHyphenationManager::nsHyphenationManager()
{
  mHyphAliases.Init();
  mPatternFiles.Init();
  mHyphenators.Init();
  LoadPatternList();
  LoadAliases();
}

nsHyphenationManager::~nsHyphenationManager()
{
  sInstance = nsnull;
}

already_AddRefed<nsHyphenator>
nsHyphenationManager::GetHyphenator(nsIAtom *aLocale)
{
  nsRefPtr<nsHyphenator> hyph;
  mHyphenators.Get(aLocale, getter_AddRefs(hyph));
  if (hyph) {
    return hyph.forget();
  }
  nsCOMPtr<nsIFile> file = mPatternFiles.Get(aLocale);
  if (!file) {
    nsCOMPtr<nsIAtom> alias = mHyphAliases.Get(aLocale);
    if (alias) {
      mHyphenators.Get(alias, getter_AddRefs(hyph));
      if (hyph) {
        return hyph.forget();
      }
      file = mPatternFiles.Get(alias);
      if (file) {
        aLocale = alias;
      }
    }
    if (!file) {
      // In the case of a locale such as "de-DE-1996", we try replacing
      // successive trailing subtags with "-*" to find fallback patterns,
      // so "de-DE-1996" -> "de-DE-*" (and then recursively -> "de-*")
      nsAtomCString localeStr(aLocale);
      if (StringEndsWith(localeStr, NS_LITERAL_CSTRING("-*"))) {
        localeStr.Truncate(localeStr.Length() - 2);
      }
      PRInt32 i = localeStr.RFindChar('-');
      if (i > 1) {
        localeStr.Replace(i, localeStr.Length() - i, "-*");
        nsCOMPtr<nsIAtom> fuzzyLocale = do_GetAtom(localeStr);
        return GetHyphenator(fuzzyLocale);
      } else {
        return nsnull;
      }
    }
  }
  hyph = new nsHyphenator(file);
  if (hyph->IsValid()) {
    mHyphenators.Put(aLocale, hyph);
    return hyph.forget();
  }
#ifdef DEBUG
  nsCString msg;
  file->GetNativePath(msg);
  msg.Insert("failed to load patterns from ", 0);
  NS_WARNING(msg.get());
#endif
  mPatternFiles.Remove(aLocale);
  return nsnull;
}

void
nsHyphenationManager::LoadPatternList()
{
  mPatternFiles.Clear();
  mHyphenators.Clear();
  
  nsresult rv;
  
  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc) {
    return;
  }
  
  nsCOMPtr<nsIFile> greDir;
  rv = dirSvc->Get(NS_GRE_DIR,
                   NS_GET_IID(nsIFile), getter_AddRefs(greDir));
  if (NS_SUCCEEDED(rv)) {
    greDir->AppendNative(NS_LITERAL_CSTRING("hyphenation"));
    LoadPatternListFromDir(greDir);
  }
  
  nsCOMPtr<nsIFile> appDir;
  rv = dirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                   NS_GET_IID(nsIFile), getter_AddRefs(appDir));
  if (NS_SUCCEEDED(rv)) {
    appDir->AppendNative(NS_LITERAL_CSTRING("hyphenation"));
    bool equals;
    if (NS_SUCCEEDED(appDir->Equals(greDir, &equals)) && !equals) {
      LoadPatternListFromDir(appDir);
    }
  }
}

void
nsHyphenationManager::LoadPatternListFromDir(nsIFile *aDir)
{
  nsresult rv;
  
  bool check = false;
  rv = aDir->Exists(&check);
  if (NS_FAILED(rv) || !check) {
    return;
  }
  
  rv = aDir->IsDirectory(&check);
  if (NS_FAILED(rv) || !check) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(e));
  if (NS_FAILED(rv)) {
    return;
  }
  
  nsCOMPtr<nsIDirectoryEnumerator> files(do_QueryInterface(e));
  if (!files) {
    return;
  }
  
  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(file))) && file){
    nsAutoString dictName;
    file->GetLeafName(dictName);
    NS_ConvertUTF16toUTF8 locale(dictName);
    ToLowerCase(locale);
    if (!StringEndsWith(locale, NS_LITERAL_CSTRING(".dic"))) {
      continue;
    }
    if (StringBeginsWith(locale, NS_LITERAL_CSTRING("hyph_"))) {
      locale.Cut(0, 5);
    }
    locale.SetLength(locale.Length() - 4); // strip ".dic"
    for (PRUint32 i = 0; i < locale.Length(); ++i) {
      if (locale[i] == '_') {
        locale.Replace(i, 1, '-');
      }
    }
#ifdef DEBUG_hyph
    printf("adding hyphenation patterns for %s: %s\n", locale.get(),
           NS_ConvertUTF16toUTF8(dictName).get());
#endif
    nsCOMPtr<nsIAtom> localeAtom = do_GetAtom(locale);
    mPatternFiles.Put(localeAtom, file);
  }
}

void
nsHyphenationManager::LoadAliases()
{
  nsIPrefBranch* prefRootBranch = Preferences::GetRootBranch();
  if (!prefRootBranch) {
    return;
  }
  PRUint32 prefCount;
  char **prefNames;
  nsresult rv = prefRootBranch->GetChildList(INTL_HYPHENATIONALIAS_PREFIX,
                                             &prefCount, &prefNames);
  if (NS_SUCCEEDED(rv) && prefCount > 0) {
    for (PRUint32 i = 0; i < prefCount; ++i) {
      nsAdoptingCString value = Preferences::GetCString(prefNames[i]);
      if (value) {
        nsCAutoString alias(prefNames[i]);
        alias.Cut(0, strlen(INTL_HYPHENATIONALIAS_PREFIX));
        ToLowerCase(alias);
        ToLowerCase(value);
        nsCOMPtr<nsIAtom> aliasAtom = do_GetAtom(alias);
        nsCOMPtr<nsIAtom> valueAtom = do_GetAtom(value);
        mHyphAliases.Put(aliasAtom, valueAtom);
      }
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefNames);
  }
}
