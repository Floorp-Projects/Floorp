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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein <Deinst@world.std.com>
 *                 Kevin Hendricks <kevin.hendricks@sympatico.ca>
 *                 Michiel van Leeuwen <mvl@exedo.nl>
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
 *  This spellchecker is based on the MySpell spellchecker made for Open Office
 *  by Kevin Hendricks.  Although the algorithms and code, have changed 
 *  slightly, the architecture is still the same. The Mozilla implementation
 *  is designed to be compatible with the Open Office dictionaries.
 *  Please do not make changes to the affix or dictionary file formats 
 *  without attempting to coordinate with Kevin.  For more information 
 *  on the original MySpell see 
 *  http://whiteboard.openoffice.org/source/browse/whiteboard/lingucomponent/source/spellcheck/myspell/
 *
 *  A special thanks and credit goes to Geoff Kuenning
 * the creator of ispell.  MySpell's affix algorithms were
 * based on those of ispell which should be noted is
 * copyright Geoff Kuenning et.al. and now available
 * under a BSD style license. For more information on ispell
 * and affix compression in general, please see:
 * http://www.cs.ucla.edu/ficus-members/geoff/ispell.html
 * (the home page for ispell)
 *
 * ***** END LICENSE BLOCK ***** */

/* based on MySpell (c) 2001 by Kevin Hendicks  */

#include "mozMySpell.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozISpellI18NManager.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include <stdlib.h>

const PRInt32 kFirstDirSize=8;

NS_IMPL_ISUPPORTS1(mozMySpell, mozISpellCheckingEngine)

mozMySpell::mozMySpell()
{
  mMySpell = NULL;
}

mozMySpell::~mozMySpell()
{
  mPersonalDictionary = nsnull;
  delete mMySpell;
}

/* attribute wstring dictionary; */
NS_IMETHODIMP mozMySpell::GetDictionary(PRUnichar **aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  *aDictionary = ToNewUnicode(mDictionary);
  return *aDictionary ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* set the Dictionary.
 * This also Loads the dictionary and initializes the converter using the dictionaries converter
 */
NS_IMETHODIMP mozMySpell::SetDictionary(const PRUnichar *aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  nsresult rv = NS_OK;
  
  if (*aDictionary && !mDictionary.Equals(aDictionary)) {
    mDictionary = aDictionary;

    nsAutoString affFileName, dictFileName;

    // XXX This isn't really good. nsIFile->Path isn't xp save etc.
    // see nsIFile.idl
    // A better way would be to QU ti nsILocalFile, and get a filehandle
    // from there. Only problem is that myspell wants a path

    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!file)
      return NS_ERROR_FAILURE;
    rv = file->Append(NS_LITERAL_STRING("myspell"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = file->Append(mDictionary + NS_LITERAL_STRING(".aff"));
    NS_ENSURE_SUCCESS(rv, rv);
    file->GetPath(affFileName);

    rv = NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!file)
      return NS_ERROR_FAILURE;
    rv = file->Append(NS_LITERAL_STRING("myspell"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = file->Append(mDictionary + NS_LITERAL_STRING(".dic"));
    NS_ENSURE_SUCCESS(rv, rv);
    file->GetPath(dictFileName);

    // SetDictionary can be called multiple times, so we might have a valid mMySpell instance 
    // which needs cleaned up.
    if (mMySpell)
      delete mMySpell;

    mMySpell = new MySpell(NS_ConvertUTF16toUTF8(affFileName).get(), NS_ConvertUTF16toUTF8(dictFileName).get());
    if (!mMySpell)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ccm->GetUnicodeDecoder(mMySpell->get_dic_encoding(), getter_AddRefs(mDecoder));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ccm->GetUnicodeEncoder(mMySpell->get_dic_encoding(), getter_AddRefs(mEncoder));
    if (mEncoder && NS_SUCCEEDED(rv)) {
      mEncoder->SetOutputErrorBehavior(mEncoder->kOnError_Signal, nsnull, '?');
    }

    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 pos = mDictionary.FindChar('-');
    if (pos == -1)
      mLanguage.Assign(NS_LITERAL_STRING("en"));      
    else
      mLanguage = Substring(mDictionary, 0, pos);
  }
  
  return rv;
}

/* readonly attribute wstring language; */
NS_IMETHODIMP mozMySpell::GetLanguage(PRUnichar **aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  *aLanguage = ToNewUnicode(mLanguage);
  return *aLanguage ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute boolean providesPersonalDictionary; */
NS_IMETHODIMP mozMySpell::GetProvidesPersonalDictionary(PRBool *aProvidesPersonalDictionary)
{
  NS_ENSURE_ARG_POINTER(aProvidesPersonalDictionary);

  *aProvidesPersonalDictionary = PR_FALSE;
  return NS_OK;
}

/* readonly attribute boolean providesWordUtils; */
NS_IMETHODIMP mozMySpell::GetProvidesWordUtils(PRBool *aProvidesWordUtils)
{
  NS_ENSURE_ARG_POINTER(aProvidesWordUtils);

  *aProvidesWordUtils = PR_FALSE;
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP mozMySpell::GetName(PRUnichar * *aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute wstring copyright; */
NS_IMETHODIMP mozMySpell::GetCopyright(PRUnichar * *aCopyright)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute mozIPersonalDictionary personalDictionary; */
NS_IMETHODIMP mozMySpell::GetPersonalDictionary(mozIPersonalDictionary * *aPersonalDictionary)
{
  *aPersonalDictionary = mPersonalDictionary;
  NS_IF_ADDREF(*aPersonalDictionary);
  return NS_OK;
}

NS_IMETHODIMP mozMySpell::SetPersonalDictionary(mozIPersonalDictionary * aPersonalDictionary)
{
  mPersonalDictionary = aPersonalDictionary;
  return NS_OK;
}

/* void GetDictionaryList ([array, size_is (count)] out wstring dictionaries, out PRUint32 count); */
NS_IMETHODIMP mozMySpell::GetDictionaryList(PRUnichar ***aDictionaries, PRUint32 *aCount)
{
  if (!aDictionaries || !aCount)
    return NS_ERROR_NULL_POINTER;

  *aDictionaries = 0;
  *aCount = 0;
  PRInt32 tempCount=0, arraySize = kFirstDirSize;
  PRUnichar **newPtr;

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!file)
    return NS_ERROR_FAILURE;

  rv = file->Append(NS_LITERAL_STRING("myspell"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> dirEntries;
  rv = file->GetDirectoryEntries(getter_AddRefs(dirEntries));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!dirEntries)
    return NS_ERROR_FAILURE;

  PRUnichar **tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * kFirstDirSize);
  if (!tmpPtr)
    return NS_ERROR_OUT_OF_MEMORY;

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(dirEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> nextItem;

    dirEntries->GetNext(getter_AddRefs(nextItem));
    nsCOMPtr<nsIFile> theFile = do_QueryInterface(nextItem);
    
    if (theFile) {
      nsAutoString fileName;
      theFile->GetLeafName(fileName);
      PRInt32 dotLocation = fileName.FindChar('.');
      if ((dotLocation != -1) &&
          Substring(fileName,dotLocation,4).EqualsLiteral(".dic")) {
        if (tempCount >= arraySize) {
          arraySize = 2 * tempCount;
          newPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * arraySize);
          if (!newPtr){
            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(tempCount, tmpPtr);
            return NS_ERROR_OUT_OF_MEMORY;
          }
          for (PRInt32 i = 0; i < tempCount; ++i){
            newPtr[i] = tmpPtr[i];
          }
          nsMemory::Free(tmpPtr);
          tmpPtr=newPtr;
        }
        tmpPtr[tempCount++] = ToNewUnicode(Substring(fileName,0,dotLocation));
      }
    }
  }

  *aDictionaries = tmpPtr;
  *aCount = tempCount;
  return NS_OK;
}

nsresult mozMySpell::ConvertCharset(const PRUnichar* aStr, char ** aDst)
{
  NS_ENSURE_ARG_POINTER(aDst);
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_NULL_POINTER);

  PRInt32 outLength;
  PRInt32 inLength = nsCRT::strlen(aStr);
  nsresult rv = mEncoder->GetMaxLength(aStr, inLength, &outLength);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDst = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
  NS_ENSURE_TRUE(*aDst, NS_ERROR_OUT_OF_MEMORY);

  rv = mEncoder->Convert(aStr, &inLength, *aDst, &outLength);
  if (NS_SUCCEEDED(rv))
    (*aDst)[outLength] = '\0'; 

  return rv;
}

/* boolean Check (in wstring word); */
NS_IMETHODIMP mozMySpell::Check(const PRUnichar *aWord, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mMySpell, NS_ERROR_FAILURE);

  nsXPIDLCString charsetWord;
  nsresult rv = ConvertCharset(aWord, getter_Copies(charsetWord));
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = mMySpell->spell(charsetWord);


  if (!*aResult && mPersonalDictionary) 
    rv = mPersonalDictionary->Check(aWord, mLanguage.get(), aResult);
  
  return rv;
}

/* void Suggest (in wstring word, [array, size_is (count)] out wstring suggestions, out PRUint32 count); */
NS_IMETHODIMP mozMySpell::Suggest(const PRUnichar *aWord, PRUnichar ***aSuggestions, PRUint32 *aSuggestionCount)
{
  NS_ENSURE_ARG_POINTER(aSuggestions);
  NS_ENSURE_ARG_POINTER(aSuggestionCount);
  NS_ENSURE_TRUE(mMySpell, NS_ERROR_FAILURE);

  nsresult rv;
  *aSuggestionCount = 0;
  
  nsXPIDLCString charsetWord;
  rv = ConvertCharset(aWord, getter_Copies(charsetWord));
  NS_ENSURE_SUCCESS(rv, rv);

  char ** wlst;
  *aSuggestionCount = mMySpell->suggest(&wlst, charsetWord);

  if (*aSuggestionCount) {    
    *aSuggestions  = (PRUnichar **)nsMemory::Alloc(*aSuggestionCount * sizeof(PRUnichar *));    
    if (*aSuggestions) {
      PRUint32 index = 0;
      for (index = 0; index < *aSuggestionCount && NS_SUCCEEDED(rv); ++index) {
        // Convert the suggestion to utf16     
        PRInt32 inLength = nsCRT::strlen(wlst[index]);
        PRInt32 outLength;
        rv = mDecoder->GetMaxLength(wlst[index], inLength, &outLength);
        if (NS_SUCCEEDED(rv))
        {
          (*aSuggestions)[index] = (PRUnichar *) nsMemory::Alloc(sizeof(PRUnichar) * (outLength+1));
          if ((*aSuggestions)[index])
          {
            rv = mDecoder->Convert(wlst[index], &inLength, (*aSuggestions)[index], &outLength);
            if (NS_SUCCEEDED(rv))
              (*aSuggestions)[index][outLength] = 0;
          } 
          else
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
      }

      if (NS_FAILED(rv))
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(index, *aSuggestions); // free the PRUnichar strings up to the point at which the error occurred
    }
    else // if (*aSuggestions)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(*aSuggestionCount, wlst);
  return rv;
}
