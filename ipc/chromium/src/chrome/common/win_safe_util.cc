// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <shobjidl.h>
#include <atlcomcli.h>

#include "chrome/common/win_safe_util.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/win_util.h"

namespace win_util {

// This function implementation is based on the attachment execution
// services functionally deployed with IE6 or Service pack 2. This
// functionality is exposed in the IAttachmentExecute COM interface.
// more information at:
// http://msdn2.microsoft.com/en-us/library/ms647048.aspx
bool SaferOpenItemViaShell(HWND hwnd, const std::wstring& window_title,
                           const FilePath& full_path,
                           const std::wstring& source_url,
                           bool ask_for_app) {
  ATL::CComPtr<IAttachmentExecute> attachment_services;
  HRESULT hr = attachment_services.CoCreateInstance(CLSID_AttachmentServices);
  if (FAILED(hr)) {
    // We don't have Attachment Execution Services, it must be a pre-XP.SP2
    // Windows installation, or the thread does not have COM initialized.
    if (hr == CO_E_NOTINITIALIZED) {
      NOTREACHED();
      return false;
    }
    return OpenItemViaShell(full_path, ask_for_app);
  }

  // This GUID is associated with any 'don't ask me again' settings that the
  // user can select for different file types.
  // {2676A9A2-D919-4fee-9187-152100393AB2}
  static const GUID kClientID = { 0x2676a9a2, 0xd919, 0x4fee,
    { 0x91, 0x87, 0x15, 0x21, 0x0, 0x39, 0x3a, 0xb2 } };

  attachment_services->SetClientGuid(kClientID);

  if (!window_title.empty())
    attachment_services->SetClientTitle(window_title.c_str());

  // To help windows decide if the downloaded file is dangerous we can provide
  // what the documentation calls evidence. Which we provide now:
  //
  // Set the file itself as evidence.
  hr = attachment_services->SetLocalPath(full_path.value().c_str());
  if (FAILED(hr))
    return false;
  // Set the origin URL as evidence.
  hr = attachment_services->SetSource(source_url.c_str());
  if (FAILED(hr))
    return false;

  // Now check the windows policy.
  bool do_prompt;
  hr = attachment_services->CheckPolicy();
  if (S_FALSE == hr) {
    // The user prompt is required.
    do_prompt = true;
  } else if (S_OK == hr) {
    // An S_OK means that the file is safe to open without user consent.
    do_prompt = false;
  } else {
    // It is possible that the last call returns an undocumented result
    // equal to 0x800c000e which seems to indicate that the URL failed the
    // the security check. If you proceed with the Prompt() call the
    // Shell might show a dialog that says:
    // "windows found that this file is potentially harmful. To help protect
    // your computer, Windows has blocked access to this file."
    // Upon dismissal of the dialog windows will delete the file (!!).
    // So, we can 'return' here but maybe is best to let it happen to fail on
    // the safe side.
  }
  if (do_prompt) {
    ATTACHMENT_ACTION action;
    // We cannot control what the prompt says or does directly but it
    // is a pretty decent dialog; for example, if an excutable is signed it can
    // decode and show the publisher and the certificate.
    hr = attachment_services->Prompt(hwnd, ATTACHMENT_PROMPT_EXEC, &action);
    if (FAILED(hr) || (ATTACHMENT_ACTION_CANCEL == action))
    {
      // The user has declined opening the item.
      return false;
    }
  }
  return OpenItemViaShellNoZoneCheck(full_path, ask_for_app);
}

bool SetInternetZoneIdentifier(const FilePath& full_path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  std::wstring path = full_path.value() + L":Zone.Identifier";
  HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, kShare, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return false;

  const char kIdentifier[] = "[ZoneTransfer]\nZoneId=3";
  DWORD written = 0;
  BOOL result = WriteFile(file, kIdentifier, arraysize(kIdentifier), &written,
                          NULL);
  CloseHandle(file);

  if (!result || written != arraysize(kIdentifier)) {
    DCHECK(FALSE);
    return false;
  }

  return true;
}

}  // namespace win_util
