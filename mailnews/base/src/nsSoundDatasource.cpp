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
 * Seth Spitzer <sspitzer@netscape.com> Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsSoundDatasource.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIFileURL.h"
#include "nsISimpleEnumerator.h"
#include "nsNetCID.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

#define SOUND_ROOT "moz-mailsounds://"
#define DEFAULT_SOUND_URL "_moz_mailbeep"
#define DEFAULT_SOUND_URL_NAME "System New Mail Sound"  // move to string bundle
#define PREF_NEW_MAIL_URL "mail.biff.play_sound.url"
#define WAV_EXTENSION ".wav"
#define WAV_EXTENSION_LENGTH 4
#define FILE_SCHEME "file://"
#define FILE_SCHEME_LEN 7

#ifdef XP_WIN
#include <windows.h> 
#endif

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsSoundDatasource::nsSoundDatasource()
{
}

nsSoundDatasource::~nsSoundDatasource()
{
}

NS_IMPL_ISUPPORTS1(nsSoundDatasource, nsIRDFDataSource)

nsresult
nsSoundDatasource::Init()
{
  nsresult rv;

  mRDFService = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                                getter_AddRefs(kNC_Child));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),
                                getter_AddRefs(kNC_Name));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "SoundURL"),
                                getter_AddRefs(kNC_SoundURL));
  NS_ENSURE_SUCCESS(rv,rv);
	return NS_OK;
}

NS_IMETHODIMP 
nsSoundDatasource::GetURI(char **aURI)
{
  if ((*aURI = nsCRT::strdup("rdf:mailsounds")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP 
nsSoundDatasource::GetSource(nsIRDFResource *property, nsIRDFNode *target, PRBool tv, nsIRDFResource **source)
{
  NS_ENSURE_ARG_POINTER(property);
  NS_ENSURE_ARG_POINTER(target);
  NS_ENSURE_ARG_POINTER(source);
  
  *source = nsnull;
  return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsSoundDatasource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                PRBool tv,
                                nsIRDFNode **target /* out */)
{
	nsresult rv = NS_RDF_NO_VALUE;

	NS_ENSURE_ARG_POINTER(property);
  NS_ENSURE_ARG_POINTER(target);
  NS_ENSURE_ARG_POINTER(source);
  
	*target = nsnull;

	// we only have positive assertions in the sound data source.
	if (!tv) 
    return NS_RDF_NO_VALUE;

  nsXPIDLCString value;
  rv = source->GetValue(getter_Copies(value));
  NS_ENSURE_SUCCESS(rv,rv);

  if (property == kNC_Name.get()) {
    nsCOMPtr<nsIRDFLiteral> name;
    if (strcmp(value.get(), DEFAULT_SOUND_URL)) {
      const char *lastSlash = strrchr(value.get(), '/');
      // turn "file://C|/winnt/media/foo.wav" into "foo".
      nsCAutoString soundName(lastSlash + 1);
      soundName.Truncate(soundName.Length() - WAV_EXTENSION_LENGTH);
      rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(soundName).get(), getter_AddRefs(name));
    }
    else {
      rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(DEFAULT_SOUND_URL_NAME).get(), getter_AddRefs(name));
    }
    NS_ENSURE_SUCCESS(rv,rv);
    
    if (!name) 
      return NS_RDF_NO_VALUE;
    else
      return name->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
  }
  else if (property == kNC_SoundURL.get()) {
    nsCOMPtr<nsIRDFLiteral> name;
    rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(value).get(), getter_AddRefs(name));
    NS_ENSURE_SUCCESS(rv,rv);
    
    if (!name) 
      return NS_RDF_NO_VALUE;
    else
      return name->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
  }
  else if (property == kNC_Child.get()) {
    if (strcmp(value.get(),SOUND_ROOT) == 0) {
      nsCOMPtr <nsIRDFResource> childResource;
      rv = mRDFService->GetResource(NS_LITERAL_CSTRING(DEFAULT_SOUND_URL),
                                    getter_AddRefs(childResource));
      NS_ENSURE_SUCCESS(rv,rv);
      return childResource->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
    }
    else {
      return NS_RDF_NO_VALUE;
    }
  }
  else {
    // do nothing
  }
	return(NS_RDF_NO_VALUE);
}

#ifdef XP_WIN
BYTE * GetValueBytes( HKEY hKey, const char *pValueName)
{
	LONG	err;
	DWORD	bufSz;
	LPBYTE	pBytes = NULL;

	err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, NULL, &bufSz); 
	if (err == ERROR_SUCCESS) {
		pBytes = new BYTE[bufSz];
		err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, pBytes, &bufSz);
		if (err != ERROR_SUCCESS) {
			delete [] pBytes;
			pBytes = NULL;
		}
	}
	return( pBytes);
}
#endif

NS_IMETHODIMP
nsSoundDatasource::GetTargets(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsISimpleEnumerator **targets /* out */)
{
	nsresult rv = NS_OK;

	NS_ENSURE_ARG_POINTER(property);
  NS_ENSURE_ARG_POINTER(targets);
  NS_ENSURE_ARG_POINTER(source);

  *targets = nsnull;

	// we only have positive assertions in the sound data source.
	if (!tv) 
    return NS_RDF_NO_VALUE;

  nsXPIDLCString value;
  rv = source->GetValue(getter_Copies(value));
  NS_ENSURE_SUCCESS(rv,rv);

  if (property == kNC_Child.get()) {
    if (strcmp(value.get(), SOUND_ROOT))
      return NS_NewEmptyEnumerator(targets);

    nsCOMPtr<nsISupportsArray> children;
    rv = NS_NewISupportsArray(getter_AddRefs(children));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsIRDFResource> res;
    rv = mRDFService->GetResource(NS_LITERAL_CSTRING(DEFAULT_SOUND_URL), getter_AddRefs(res));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = children->AppendElement(res);
    NS_ENSURE_SUCCESS(rv,rv);

#ifdef XP_WIN
    nsCAutoString soundFolder;
    
    HKEY sKey;
	  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS) {
		  BYTE *pBytes = GetValueBytes( sKey, "MediaPath");
	  	if (pBytes) {
		  	soundFolder = (const char *)pBytes;
		  	delete [] pBytes;
      }
		  ::RegCloseKey( sKey);
    }

    nsCOMPtr <nsILocalFile> directory = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = directory->InitWithPath(NS_ConvertASCIItoUCS2(soundFolder));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsISimpleEnumerator> dirEntries;
    rv = directory->GetDirectoryEntries(getter_AddRefs(dirEntries));
    NS_ENSURE_SUCCESS(rv,rv);

    if (dirEntries) {
      PRBool hasMore = PR_FALSE;
      PRInt32 dirCount = 0, fileCount = 0;
 
      while (NS_SUCCEEDED(dirEntries->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr <nsISupports> nextItem;
        dirEntries->GetNext(getter_AddRefs(nextItem));
        nsCOMPtr <nsIFile> theFile = do_QueryInterface(nextItem);
   
        PRBool isDirectory;
        theFile->IsDirectory(&isDirectory);
   
        if (!isDirectory) {
          nsCOMPtr<nsIFileURL> theFileURL = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = theFileURL->SetFile(theFile);
          NS_ENSURE_SUCCESS(rv,rv);
          
          nsXPIDLCString theFileSpec;
          rv = theFileURL->GetSpec(theFileSpec);
          NS_ENSURE_SUCCESS(rv,rv);
          
          // if it doesn't end with .wav, or it contains a %20, skip it.
          if (!strstr(theFileSpec.get(),"%20") && (theFileSpec.Length() > WAV_EXTENSION_LENGTH)) {
            if (strcmp(theFileSpec.get() + theFileSpec.Length() - WAV_EXTENSION_LENGTH, WAV_EXTENSION) == 0) {
              rv = mRDFService->GetResource(theFileSpec, getter_AddRefs(res));
              NS_ENSURE_SUCCESS(rv,rv);
              
              rv = children->AppendElement(res);
              NS_ENSURE_SUCCESS(rv,rv);
            }
          }
        }
      }
    }
#endif

    // add entry for the pref specified one, if a file:// url
    // if not, it's a system sound, and we already added it.
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString soundURLSpec;
    rv = prefBranch->GetCharPref(PREF_NEW_MAIL_URL,getter_Copies(soundURLSpec));
    NS_ENSURE_SUCCESS(rv,rv);

    if (!strncmp(soundURLSpec.get(), FILE_SCHEME, FILE_SCHEME_LEN)) {
      rv = mRDFService->GetResource(soundURLSpec, getter_AddRefs(res));
      NS_ENSURE_SUCCESS(rv,rv);
      rv = children->AppendElement(res);
      NS_ENSURE_SUCCESS(rv,rv);
    }

    nsISimpleEnumerator* result = new nsArrayEnumerator(children);
    if (!result) 
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*targets = result);
    return NS_OK;
  }
  else if (property == kNC_SoundURL.get()) {
    nsCOMPtr<nsIRDFLiteral> name;
    rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(value).get(), getter_AddRefs(name));
    NS_ENSURE_SUCCESS(rv,rv);

    nsISimpleEnumerator* result = new nsSingletonEnumerator(name);
    if (!result) 
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(*targets = result);
    return NS_OK;
  }
  else if (property == kNC_Name.get()) {
    nsCOMPtr<nsIRDFLiteral> name;
    if (strcmp(value.get(), DEFAULT_SOUND_URL)) {
      const char *lastSlash = strrchr(value.get(), '/');
      // turn "file://C|/winnt/media/foo.wav" into "foo".
      nsCAutoString soundName(lastSlash + 1);
      soundName.Truncate(soundName.Length() - WAV_EXTENSION_LENGTH);
      rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(soundName).get(), getter_AddRefs(name));
    }
    else {
      rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(DEFAULT_SOUND_URL_NAME).get(), getter_AddRefs(name));
    }
    NS_ENSURE_SUCCESS(rv,rv);

    nsISimpleEnumerator* result = new nsSingletonEnumerator(name);
    if (!result) 
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(*targets = result);
    return NS_OK;
  }
  else {
    // do nothing
  }

	return NS_NewEmptyEnumerator(targets);
}

NS_IMETHODIMP
nsSoundDatasource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsSoundDatasource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsSoundDatasource::Change(nsIRDFResource* aSource,
							 nsIRDFResource* aProperty,
							 nsIRDFNode* aOldTarget,
							 nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsSoundDatasource::Move(nsIRDFResource* aOldSource,
						   nsIRDFResource* aNewSource,
						   nsIRDFResource* aProperty,
						   nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}


NS_IMETHODIMP
nsSoundDatasource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
  NS_ENSURE_ARG_POINTER(property);
  NS_ENSURE_ARG_POINTER(target);
  NS_ENSURE_ARG_POINTER(source);
  NS_ENSURE_ARG_POINTER(hasAssertion);

	*hasAssertion = PR_FALSE;

  // we only have positive assertions in the sound data source.
	if (!tv) 
    return NS_OK;

	if (property == kNC_Child.get()) {
    nsXPIDLCString value;
    nsresult rv = source->GetValue(getter_Copies(value));
    NS_ENSURE_SUCCESS(rv,rv);
    // only root has children
    *hasAssertion = (strcmp(value.get(), SOUND_ROOT) == 0);
    return NS_OK;
  }
  else if (property == kNC_Name.get() || property == kNC_SoundURL.get()) {
    // everything has a name and a url
    *hasAssertion = PR_TRUE;
  }
  else {
    // do nothing
  }

	return NS_OK;
}


NS_IMETHODIMP 
nsSoundDatasource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsSoundDatasource::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv = NS_OK;
  
  if (aArc == kNC_Child.get()) {
    nsXPIDLCString value;
    rv = source->GetValue(getter_Copies(value));
    // only root has children
    *result = (strcmp(value.get(), SOUND_ROOT) == 0);
    return NS_OK;
  }
  else if (aArc == kNC_Name.get() || aArc == kNC_SoundURL.get()) {
    *result = PR_TRUE;
    return NS_OK;
  }
  
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSoundDatasource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSoundDatasource::ArcLabelsOut(nsIRDFResource *source,
				   nsISimpleEnumerator **labels /* out */)
{
  NS_ENSURE_ARG_POINTER(labels);
  NS_ENSURE_ARG_POINTER(source);

  nsresult rv = NS_OK;

  //return NS_NewEmptyEnumerator(labels);
  
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  NS_ENSURE_SUCCESS(rv,rv);

  array->AppendElement(kNC_Name);
  array->AppendElement(kNC_SoundURL);

  nsXPIDLCString value;
  rv = source->GetValue(getter_Copies(value));
  // only root has children
  PRBool hasChildren = (strcmp(value.get(), SOUND_ROOT) == 0);

  if (hasChildren) {
   array->AppendElement(kNC_Child);
  }

  nsISimpleEnumerator* result = new nsArrayEnumerator(array);
  if (!result) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *labels = result;
  return NS_OK;
}

NS_IMETHODIMP
nsSoundDatasource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSoundDatasource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}

NS_IMETHODIMP
nsSoundDatasource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}

NS_IMETHODIMP
nsSoundDatasource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}

NS_IMETHODIMP 
nsSoundDatasource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
  NS_ASSERTION(PR_FALSE, "Not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSoundDatasource::AddObserver(nsIRDFObserver *n)
{
  NS_ENSURE_ARG_POINTER(n);

	if (!mObservers)
  {
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) return rv;
  }
  mObservers->AppendElement(n);
  return NS_OK;
}


NS_IMETHODIMP
nsSoundDatasource::RemoveObserver(nsIRDFObserver *n)
{
  NS_ENSURE_ARG_POINTER(n);
  if (!mObservers)
		return NS_OK;

	mObservers->RemoveElement(n);
	return NS_OK;
}

NS_IMETHODIMP
nsSoundDatasource::BeginUpdateBatch()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSoundDatasource::EndUpdateBatch()
{
  return NS_OK;
}
