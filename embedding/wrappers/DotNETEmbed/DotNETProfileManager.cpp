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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
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

#using <mscorlib.dll>
#include "cstringt.h"

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIProfile.h"
#include "DotNetProfileManager.h"

using namespace Mozilla::Embedding;

// static
void
ProfileManager::EnsureProfileService()
{
  if (sProfileService) {
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIProfile> profileService =
    do_GetService(NS_PROFILE_CONTRACTID, &rv);
  ThrowIfFailed(rv);

  sProfileService = profileService.get();
  NS_ADDREF(sProfileService);
}

// static
Int32
ProfileManager::get_ProfileCount() 
{ 
  EnsureProfileService();

  PRInt32 count;
  nsresult rv = sProfileService->GetProfileCount(&count);
  ThrowIfFailed(rv);

  return count;
}

// static
String *
ProfileManager::GetProfileList()[]
{
  EnsureProfileService();

  PRUint32 profileCount;
  PRUnichar **profiles;

  nsresult rv = sProfileService->GetProfileList(&profileCount, &profiles);
  ThrowIfFailed(rv);

  String *list[] = new String *[profileCount];

  for (PRUint32 i = 0; i < profileCount; ++i) {
    list[i] = CopyString(nsDependentString(profiles[i]));
  }

  return list;
}

// static
bool
ProfileManager::ProfileExists(String *aProfileName)
{
  EnsureProfileService();

  PRBool exists;
  const wchar_t __pin * profileName = PtrToStringChars(aProfileName);

  nsresult rv = sProfileService->ProfileExists(profileName, &exists);
  ThrowIfFailed(rv);

  return exists;
}

// static
String*
ProfileManager::get_CurrentProfile()
{
  EnsureProfileService();

  nsXPIDLString currentProfile;
  nsresult rv =
    sProfileService->GetCurrentProfile(getter_Copies(currentProfile));
  ThrowIfFailed(rv);

  return CopyString(currentProfile);
}

// static
void
ProfileManager::set_CurrentProfile(String* aCurrentProfile)
{
  EnsureProfileService();

  const wchar_t __pin * currentProfile = PtrToStringChars(aCurrentProfile);
  nsresult rv = sProfileService->SetCurrentProfile(currentProfile);
  ThrowIfFailed(rv);
}

// static
void
ProfileManager::ShutDownCurrentProfile(UInt32 shutDownType)
{
  EnsureProfileService();

  nsresult rv = sProfileService->ShutDownCurrentProfile(shutDownType);
  ThrowIfFailed(rv);
}

// static
void
ProfileManager::CreateNewProfile(String* aProfileName,
                                 String *aNativeProfileDir, String* aLangcode,
                                 bool useExistingDir)
{
  EnsureProfileService();

  const wchar_t __pin * profileName = PtrToStringChars(aProfileName);
  const wchar_t __pin * nativeProfileDir = PtrToStringChars(aNativeProfileDir);
  const wchar_t __pin * langCode = PtrToStringChars(aLangcode);

  nsresult rv = sProfileService->CreateNewProfile(profileName,
                                                  nativeProfileDir, langCode,
                                                  useExistingDir);
  ThrowIfFailed(rv);
}

// static
void
ProfileManager::RenameProfile(String* aOldName, String* aNewName)
{
  EnsureProfileService();

  const wchar_t __pin * oldName = PtrToStringChars(aOldName);
  const wchar_t __pin * newName = PtrToStringChars(aNewName);

  nsresult rv = sProfileService->RenameProfile(oldName, newName);
  ThrowIfFailed(rv);
}

// static
void
ProfileManager::DeleteProfile(String* aName, bool aCanDeleteFiles)
{
  EnsureProfileService();

  const wchar_t __pin * name = PtrToStringChars(aName);
  nsresult rv = sProfileService->DeleteProfile(name, aCanDeleteFiles);
  ThrowIfFailed(rv);
}

// static
void
ProfileManager::CloneProfile(String* aProfileName)
{
  EnsureProfileService();

  const wchar_t __pin * profileName = PtrToStringChars(aProfileName);

  nsresult rv = sProfileService->CloneProfile(profileName);
  ThrowIfFailed(rv);
}

