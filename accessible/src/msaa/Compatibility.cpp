/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "Compatibility.h"

#include "nsWinUtils.h"
#include "Statistics.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::a11y;

/**
 * Return true if module version is lesser than the given version.
 */
bool
IsModuleVersionLessThan(HMODULE aModuleHandle, DWORD aMajor, DWORD aMinor)
{
  PRUnichar fileName[MAX_PATH];
  ::GetModuleFileNameW(aModuleHandle, fileName, MAX_PATH);

  DWORD dummy = 0;
  DWORD length = ::GetFileVersionInfoSizeW(fileName, &dummy);

  LPBYTE versionInfo = new BYTE[length];
  ::GetFileVersionInfoW(fileName, 0, length, versionInfo);

  UINT uLen;
  VS_FIXEDFILEINFO* fixedFileInfo = NULL;
  ::VerQueryValueW(versionInfo, L"\\", (LPVOID*)&fixedFileInfo, &uLen);
  DWORD dwFileVersionMS = fixedFileInfo->dwFileVersionMS;
  DWORD dwFileVersionLS = fixedFileInfo->dwFileVersionLS;
  delete [] versionInfo;

  DWORD dwLeftMost = HIWORD(dwFileVersionMS);
  DWORD dwSecondRight = HIWORD(dwFileVersionLS);
  return (dwLeftMost < aMajor ||
    (dwLeftMost == aMajor && dwSecondRight < aMinor));
}


////////////////////////////////////////////////////////////////////////////////
// Compatibility
////////////////////////////////////////////////////////////////////////////////

PRUint32 Compatibility::sConsumers = Compatibility::UNKNOWN;

void
Compatibility::Init()
{
  // Note we collect some AT statistics/telemetry here for convenience.

  HMODULE jawsHandle = ::GetModuleHandleW(L"jhook");
  if (jawsHandle)
    sConsumers |= (IsModuleVersionLessThan(jawsHandle, 8, 2173)) ?
                   OLDJAWS : JAWS;

  if (::GetModuleHandleW(L"gwm32inc"))
    sConsumers |= WE;

  if (::GetModuleHandleW(L"dolwinhk"))
    sConsumers |= DOLPHIN;

  if (::GetModuleHandleW(L"STSA32"))
    sConsumers |= SEROTEK;

  if (::GetModuleHandleW(L"nvdaHelperRemote"))
    sConsumers |= NVDA;

  if (::GetModuleHandleW(L"OsmHooks"))
    sConsumers |= COBRA;

  if (::GetModuleHandleW(L"WebFinderRemote"))
    sConsumers |= ZOOMTEXT;

  if (::GetModuleHandleW(L"Kazahook"))
    sConsumers |= KAZAGURU;

  if (::GetModuleHandleW(L"TextExtractorImpl32") ||
      ::GetModuleHandleW(L"TextExtractorImpl64"))
    sConsumers |= YOUDAO;

  if (::GetModuleHandleW(L"uiautomation"))
    sConsumers |= UIAUTOMATION;

  // If we have a known consumer remove the unknown bit.
  if (sConsumers != Compatibility::UNKNOWN)
    sConsumers ^= Compatibility::UNKNOWN;

  // Gather telemetry
  PRUint32 temp = sConsumers;
  for (int i = 0; temp; i++) {
    if (temp & 0x1)
      statistics::A11yConsumers(i);

    temp >>= 1;
  }

  // Turn off new tab switching for Jaws and WE.
  if (sConsumers & (JAWS | OLDJAWS | WE)) {
    // Check to see if the pref for disallowing CtrlTab is already set. If so,
    // bail out (respect the user settings). If not, set it.
    if (!Preferences::HasUserValue("browser.ctrlTab.disallowForScreenReaders"))
      Preferences::SetBool("browser.ctrlTab.disallowForScreenReaders", true);
  }
}

