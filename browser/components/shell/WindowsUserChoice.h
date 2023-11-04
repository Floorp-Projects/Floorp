/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHELL_WINDOWSUSERCHOICE_H__
#define SHELL_WINDOWSUSERCHOICE_H__

#include <windows.h>

#include "ErrorList.h"  // for nsresult
#include "mozilla/UniquePtr.h"

/*
 * Check the UserChoice Hashes for https, http, .html, .htm
 *
 * This should be checked before attempting to set a new default browser via
 * the UserChoice key, to confirm our understanding of the existing hash.
 * If an incorrect hash is written, Windows will prompt the user to choose a
 * new default (or, in recent versions, it will just reset the default to Edge).
 *
 * Assuming that the existing hash value is correct (since Windows is fairly
 * diligent about replacing bad keys), if we can recompute it from scratch,
 * then we should be able to compute a correct hash for our new UserChoice key.
 *
 * @return true if we matched all the hashes, false otherwise.
 */
bool CheckBrowserUserChoiceHashes();

/*
 * Result from CheckUserChoiceHash()
 *
 * NOTE: Currently the only positive result is OK_V1 , but the enum
 * could be extended to indicate different versions of the hash.
 */
enum class CheckUserChoiceHashResult {
  OK_V1,         // Matched the current version of the hash (as of Win10 20H2).
  ERR_MISMATCH,  // The hash did not match.
  ERR_OTHER,     // Error reading or generating the hash.
};

/*
 * Generate a UserChoice Hash, compare it with the one that is stored.
 *
 * See comments on CheckBrowserUserChoiceHashes(), which calls this to check
 * each of the browser associations.
 *
 * @param aExt      File extension or protocol association to check
 * @param aUserSid  String SID of the current user
 *
 * @return Result of the check, see CheckUserChoiceHashResult
 */
CheckUserChoiceHashResult CheckUserChoiceHash(const wchar_t* aExt,
                                              const wchar_t* aUserSid);

/*
 * Get the registry path for the given association, file extension or protocol.
 *
 * @return The path, or nullptr on failure.
 */
mozilla::UniquePtr<wchar_t[]> GetAssociationKeyPath(const wchar_t* aExt);

/*
 * Get the current user's SID
 *
 * @return String SID for the user of the current process, nullptr on failure.
 */
mozilla::UniquePtr<wchar_t[]> GetCurrentUserStringSid();

/*
 * Generate the UserChoice Hash
 *
 * @param aExt          file extension or protocol being registered
 * @param aUserSid      string SID of the current user
 * @param aProgId       ProgId to associate with aExt
 * @param aTimestamp    approximate write time of the UserChoice key (within
 *                      the same minute)
 *
 * @return UserChoice Hash, nullptr on failure.
 */
mozilla::UniquePtr<wchar_t[]> GenerateUserChoiceHash(const wchar_t* aExt,
                                                     const wchar_t* aUserSid,
                                                     const wchar_t* aProgId,
                                                     SYSTEMTIME aTimestamp);

/*
 * Build a ProgID from a base and AUMI
 *
 * @param aProgIDBase   A base, such as FirefoxHTML or FirefoxURL
 * @param aAumi         The AUMI of the installation
 *
 * @return Formatted ProgID.
 */
mozilla::UniquePtr<wchar_t[]> FormatProgID(const wchar_t* aProgIDBase,
                                           const wchar_t* aAumi);

/*
 * Check that the given ProgID exists in HKCR
 *
 * @return true if it could be opened for reading, false otherwise.
 */
bool CheckProgIDExists(const wchar_t* aProgID);

/*
 * Get the ProgID registered by Windows for the given association.
 *
 * The MSIX `AppManifest.xml` declares supported protocols and file
 * type associations.  Upon installation, Windows generates
 * corresponding ProgIDs for them, of the form `AppX*`.  This function
 * retrieves those generated ProgIDs (from the Windows registry).
 *
 * @return ProgID.
 */
nsresult GetMsixProgId(const wchar_t* assoc,
                       mozilla::UniquePtr<wchar_t[]>& aProgId);

#endif  // SHELL_WINDOWSUSERCHOICE_H__
