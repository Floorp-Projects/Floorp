/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@mozilla.org>
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

#include "msgCore.h"
#include "nsMsgTagService.h"
#include "nsIPrefService.h"
#include "nsISupportsPrimitives.h"
#include "nsMsgI18N.h"
#include "nsIPrefLocalizedString.h"
#include "nsMsgDBView.h" // for labels migration
#include "nsStringEnumerator.h"

NS_IMPL_ISUPPORTS1(nsMsgTagService, nsIMsgTagService)

nsMsgTagService::nsMsgTagService()
{
  m_prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  // need to figure out how to migrate the tags only once.
  MigrateLabelsToTags();
}

nsMsgTagService::~nsMsgTagService()
{
  /* destructor code */
}

/* wstring getTagForKey (in string key); */
NS_IMETHODIMP nsMsgTagService::GetTagForKey(const nsACString &key, nsAString &_retval)
{
  nsCAutoString prefName("mailnews.tags.");
  prefName.Append(key);
  prefName.AppendLiteral(".tag");
  return GetUnicharPref(prefName.get(), _retval);
}

/* void setTagForKey (in string key); */
NS_IMETHODIMP nsMsgTagService::SetTagForKey(const nsACString &key, const nsAString &tag )
{
  nsCAutoString prefName("mailnews.tags.");
  prefName.Append(key);
  prefName.AppendLiteral(".tag");
  return SetUnicharPref(prefName.get(), tag);
}

/* void getKeyForTag (in wstring tag); */
NS_IMETHODIMP nsMsgTagService::GetKeyForTag(const nsAString &aTag, nsACString &aKey)
{
  PRUint32 count;
  char **prefList;
  nsresult rv = m_prefBranch->GetChildList("mailnews.tags.", &count, &prefList);
  NS_ENSURE_SUCCESS(rv, rv);
  // traverse the list, and look for a pref with the desired tag value.
  for (PRUint32 i = count; i--; )
  {
      // The prefname we passed to GetChildList was of the form
      // "mailnews.tags." and we are returned the descendants
      // in the form of ""mailnews.tags.<key>.tag"
      // But we only want the tags, so check that the string
      // ends with tag.
    if (StringEndsWith(nsDependentCString(prefList[i]), NS_LITERAL_CSTRING(".tag")))
    {
      nsAutoString curTag;
      GetUnicharPref(prefList[i], curTag);
      if (aTag.Equals(curTag))
      {
        aKey = Substring(nsDependentCString(prefList[i]), 14, strlen(prefList[i]) - 18);
        break;
      }
    }
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, prefList);
  return aKey.IsEmpty() ? NS_ERROR_FAILURE : NS_OK;
}

/* void addTagForKey (in string key, in wstring tag, in long color); */
NS_IMETHODIMP nsMsgTagService::AddTagForKey(const nsACString &key, const nsAString &tag, const nsACString  &color)
{
  nsCAutoString prefName("mailnews.tags.");
  prefName.Append(key);
  prefName.AppendLiteral(".tag");
  SetUnicharPref(prefName.get(), tag);
  prefName.Replace(prefName.Length() - 3, 3, NS_LITERAL_CSTRING("color"));
  m_prefBranch->SetCharPref(prefName.get(), PromiseFlatCString(color).get());
  return NS_OK;
}

/* void addTag (in wstring tag, in long color); */
NS_IMETHODIMP nsMsgTagService::AddTag(const nsAString &tag, const nsACString  &color)
{
  nsCAutoString prefName("mailnews.tags.");
  // figure out key from tag. Apply transformation stripping out
  // illegal characters like <SP> and then convert to imap mod utf7.
  // Then, check if we have a tag with that key yet, and if so,
  // make it unique by appending -1, -2, etc.
  // Should we use an iterator?
  nsAutoString transformedTag(tag);
  transformedTag.ReplaceChar(" ()/{%*<>\\\"", '_');
  nsCAutoString key;
  CopyUTF16toMUTF7(transformedTag, key);
  prefName.Append(key);
  while (PR_TRUE)
  {
    nsAutoString tagValue;
    GetUnicharPref(prefName.get(), tagValue);
    if (tagValue.IsEmpty() || tagValue.Equals(tag))
      return AddTagForKey(key, tag, color);
    prefName.Append('A');
  }
  NS_ASSERTION(PR_FALSE, "can't get here");
  return NS_ERROR_FAILURE;
}

/* long getColorForKey (in string key); */
NS_IMETHODIMP nsMsgTagService::GetColorForKey(const nsACString &key, nsACString  &_retval)
{
  nsCAutoString prefName("mailnews.tags.");
  prefName.Append(key);
  prefName.AppendLiteral(".color");
  nsXPIDLCString color;
  return m_prefBranch->GetCharPref(prefName.get(), getter_Copies(color));
  _retval = color;
}

/* void deleteTag (in wstring tag); */
NS_IMETHODIMP nsMsgTagService::DeleteTag(const nsAString &tag)
{
  // do we want to set a .deleted pref, or just set the tag
  // property to "", or clear the pref(s)?
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIStringEnumerator tagEnumerator; */
NS_IMETHODIMP nsMsgTagService::GetTagEnumerator(nsIStringEnumerator * *aTagEnumerator)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 count;
  char **prefList;
  rv = prefBranch->GetChildList("mailnews.tags.", &count, &prefList);
  NS_ENSURE_SUCCESS(rv, rv);
  nsStringArray *stringArray = new nsStringArray(count); // or should it be count / 2?
  if (!stringArray)
    return NS_ERROR_OUT_OF_MEMORY;
  // traverse the list, and truncate all the descendant strings to just
  // one branch level below the root branch.
  for (PRUint32 i = count; i--; )
  {
      // The prefname we passed to GetChildList was of the form
      // "mailnews.tags." and we are returned the descendants
      // in the form of ""mailnews.tags.<key>.tag"
      // But we only want the tags, so check that the string
      // ends with tag.
    if (StringEndsWith(nsDependentCString(prefList[i]), NS_LITERAL_CSTRING(".tag")))
    {
      nsAutoString tag;
      GetUnicharPref(prefList[i], tag);
      stringArray->AppendString(tag);
    }
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, prefList);
  return NS_NewAdoptingStringEnumerator(aTagEnumerator, stringArray);;
}

NS_IMETHODIMP nsMsgTagService::GetKeyEnumerator(nsIUTF8StringEnumerator * *aKeyEnumerator)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 count;
  char **prefList;
  rv = prefBranch->GetChildList("mailnews.tags.", &count, &prefList);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCStringArray *stringArray = new nsCStringArray; // or should it be count / 2?
  if (!stringArray)
    return NS_ERROR_OUT_OF_MEMORY;
  // traverse the list, and truncate all the descendant strings to just
  // one branch level below the root branch.
  for (PRUint32 i = count; i--; )
  {
      // The prefname we passed to GetChildList was of the form
      // "mailnews.tags." and we are returned the descendants
      // in the form of ""mailnews.tags.<key>.tag"
      // But we only want the keys, so check that the string
      // ends with tag.
    if (StringEndsWith(nsDependentCString(prefList[i]), NS_LITERAL_CSTRING(".tag")))
    {
      nsDependentCSubstring key(nsDependentCString(prefList[i]), 14, strlen(prefList[i]) - 18);
      stringArray->AppendCString(key);
    }
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, prefList);
  return NS_NewAdoptingUTF8StringEnumerator(aKeyEnumerator, stringArray);;
}

/* End of implementation class template. */

nsresult
nsMsgTagService::getPrefService() 
{
  if (m_prefBranch) 
    return NS_OK;

  nsresult rv;
  m_prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    m_prefBranch = nsnull;

  return rv;
}

nsresult nsMsgTagService::SetUnicharPref(const char *prefName,
                              const nsAString &val)
{
  nsresult rv = getPrefService();
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!val.IsEmpty())
  {
    nsCOMPtr<nsISupportsString> supportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    if (supportsString) 
    {
      supportsString->SetData(val);
      rv = m_prefBranch->SetComplexValue(prefName,
                                         NS_GET_IID(nsISupportsString),
                                         supportsString);
    }
  }
  else 
  {
    m_prefBranch->ClearUserPref(prefName);
  }
  return rv;
}

nsresult nsMsgTagService::GetUnicharPref(const char *prefName,
                              nsAString &prefValue)
{
  nsresult rv = getPrefService();
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsISupportsString> supportsString =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  if (supportsString) 
  {
    rv = m_prefBranch->GetComplexValue(prefName,
                                       NS_GET_IID(nsISupportsString),
                                       getter_AddRefs(supportsString));
    if (supportsString)
      rv = supportsString->GetData(prefValue);
    else
      prefValue.Truncate();
  }
  return rv;
}


nsresult nsMsgTagService::MigrateLabelsToTags()
{
  nsCString prefString;

  PRInt32 prefVersion = 0;
  nsresult rv = m_prefBranch->GetIntPref("mailnews.tags.version", &prefVersion);
  if (NS_SUCCEEDED(rv) && prefVersion == 1)
    return rv;
  nsCOMPtr<nsIPrefLocalizedString> pls;
  nsXPIDLString ucsval;
  nsCAutoString labelKey("$label1");
  for(PRInt32 i = 0; i < PREF_LABELS_MAX; )
  {
    prefString.Assign(PREF_LABELS_DESCRIPTION);
    prefString.AppendInt(i + 1);
    rv = m_prefBranch->GetComplexValue(prefString.get(), NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(pls));
    NS_ENSURE_SUCCESS(rv, rv);
    pls->ToString(getter_Copies(ucsval));

    prefString.Assign(PREF_LABELS_COLOR);
    prefString.AppendInt(i + 1);
    nsXPIDLCString csval;

    NS_ENSURE_SUCCESS(rv, rv);

    rv = m_prefBranch->GetCharPref(prefString.get(), getter_Copies(csval));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AddTagForKey(labelKey, ucsval, csval);
    labelKey.SetCharAt(++i + '1', 6);
  }
  m_prefBranch->SetIntPref("mailnews.tags.version", 1);
  return rv;

}
