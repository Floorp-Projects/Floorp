/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#using <mscorlib.dll>
#include "cstringt.h"

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIProfile.h"
#include "umProfileManager.h"
#include "DotNetProfileManager.h"

using namespace System;
using namespace System::Runtime::InteropServices;
using namespace Mozilla::Embedding::ProfileManager;

#pragma unmanaged
unmanagedProfile::unmanagedProfile()
{
  mProfileService = nsnull;
}

unmanagedProfile::~unmanagedProfile()
{
}

nsresult unmanagedProfile::CreateProfileService()
{
  nsresult rv = NS_OK;
  mProfileService = do_GetService(NS_PROFILE_CONTRACTID, &rv);
  return rv;
}

long unmanagedProfile::GetProfileCount()
{
  nsresult rv = PR_TRUE;
  PRInt32 profCount = 0;

  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return 0L;

  rv = mProfileService->GetProfileCount(&profCount);
  if (NS_FAILED(rv))
    return 0L;

  return profCount;
}

PRBool unmanagedProfile::ProfileExists(const wchar_t * aProfileName)
{
  nsresult rv = PR_TRUE;
  PRBool exists = PR_FALSE;

  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return PR_FALSE;

  rv = mProfileService->ProfileExists(aProfileName, &exists);
  if (NS_FAILED(rv))
    return PR_FALSE;

  return exists;
}

const PRUnichar* unmanagedProfile::GetCurrentProfile()
{
  nsresult rv = PR_TRUE;
  nsXPIDLString curProfileName;

  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mProfileService->GetCurrentProfile(getter_Copies(curProfileName));
  if (NS_FAILED(rv)) 
    return nsnull;

  return (const PRUnichar *)curProfileName;
}

bool unmanagedProfile::SetCurrentProfile(const wchar_t * aProfileName)
{
  nsresult rv = PR_TRUE;

  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return false;

  rv = mProfileService->SetCurrentProfile(aProfileName);
  if (NS_FAILED(rv))
    return false;

  return true;
}

void unmanagedProfile::ShutDownCurrentProfile(PRUint32 shutDownType)
{
  nsresult rv = PR_TRUE;
  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return;

  mProfileService->ShutDownCurrentProfile(shutDownType);
}

PRBool unmanagedProfile::CreateNewProfile(const wchar_t * profileName, const wchar_t * nativeProfileDir, const wchar_t * langcode, PRBool useExistingDir)
{
  nsresult rv = PR_TRUE;
  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return PR_FALSE;

  rv = mProfileService->CreateNewProfile(profileName, nativeProfileDir, langcode, useExistingDir);
  if (NS_FAILED(rv))
    return PR_FALSE;
}

void unmanagedProfile::RenameProfile(const wchar_t * oldName, const wchar_t * newName)
{
  nsresult rv = PR_TRUE;
  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return;

  mProfileService->RenameProfile(oldName, newName);
}

void unmanagedProfile::DeleteProfile(const wchar_t * name, PRBool canDeleteFiles)
{
  nsresult rv = NS_OK;
  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return;

  mProfileService->DeleteProfile(name, canDeleteFiles);
}

void unmanagedProfile::CloneProfile(const wchar_t * profileName)
{
  nsresult rv = NS_OK;
  if (!mProfileService)
    rv = CreateProfileService();
  if (NS_FAILED(rv))
    return;

  mProfileService->CloneProfile(profileName);
}

#pragma managed
Profile::Profile()
{
  mProfile = NULL;
}

Profile::~Profile()
{
  Dispose(false);
}

void Profile::Dispose()
{
  Dispose(true);
}

void Profile::Dispose(bool disposing)
{
  if (mProfile)   
  {
    delete mProfile;
    mProfile = NULL;
  }

  if (disposing)  
  {
    GC::SuppressFinalize(this);
  }
}

bool Profile::CreateUnmanagedProfile()
{
  bool ret = false;

  try 
  {
    mProfile = new unmanagedProfile();
    if (mProfile)
      ret = true;
  }
  catch (Exception*) 
  {
    throw;
  }

  return ret;
}

Int32 Profile::get_ProfileCount() 
{ 
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return 0;
  }

  return mProfile->GetProfileCount();
}

void Profile::GetProfileList(UInt32* length, String** profileNames)
{
  // ToDo
  return;
}

bool Profile::ProfileExists(String *aProfileName)
{
  bool ret = false;

  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return false;
  }

  try 
  {
    const wchar_t __pin * pName = PtrToStringChars(aProfileName);
    ret = mProfile->ProfileExists(pName);
  }
  catch (Exception*) 
  {
    throw;
  }

  return ret;
}

String* Profile::get_CurrentProfile()
{
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return NULL;
  }

  return Marshal::PtrToStringUni((wchar_t *)mProfile->GetCurrentProfile());
}

void Profile::set_CurrentProfile(String* aCurrentProfile)
{
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return;
  }

  try 
  {
    const wchar_t __pin * pName = PtrToStringChars(aCurrentProfile);
    mProfile->SetCurrentProfile(pName);
  }
  catch (Exception*) 
  {
    throw;
  }
}

void Profile::ShutDownCurrentProfile(UInt32 shutDownType)
{
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return;
  }

  mProfile->ShutDownCurrentProfile(shutDownType);
}

bool Profile::CreateNewProfile(String* aProfileName, String *aNativeProfileDir, String* aLangcode, bool useExistingDir)
{
  bool ret = false;

  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return false;
  }

  try 
  {
    const wchar_t __pin * pName = PtrToStringChars(aProfileName);
    const wchar_t __pin * pDir = PtrToStringChars(aNativeProfileDir);
    const wchar_t __pin * pLang = PtrToStringChars(aLangcode);
    ret = mProfile->CreateNewProfile(pName, pDir, pLang, useExistingDir);
  }
  catch (Exception*) 
  {
      throw;
  }

  return ret;
}

void Profile::RenameProfile(String* aOldName, String* aNewName)
{
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return;
  }

  try 
  {
    const wchar_t __pin * pOld = PtrToStringChars(aOldName);
    const wchar_t __pin * pNew = PtrToStringChars(aNewName);
    mProfile->RenameProfile(pOld, pNew);
  }
  catch (Exception*) 
  {
    throw;
  }
}

void Profile::DeleteProfile(String* aName, bool aCanDeleteFiles)
{
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return;
  }

  try 
  {
    const wchar_t __pin * pName = PtrToStringChars(aName);
    mProfile->DeleteProfile(pName, aCanDeleteFiles);
  }
  catch (Exception*) 
  {
    throw;
  }
}

void Profile::CloneProfile(String* aProfileName)
{
  if (!mProfile) 
  {
    if (!CreateUnmanagedProfile()) 
      return;
  }

  try 
  {
    const wchar_t __pin * pName = PtrToStringChars(aProfileName);
    mProfile->CloneProfile(pName);
  }
  catch (Exception*) 
  {
    throw;
  }
}

