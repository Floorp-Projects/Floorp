/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFCDMEXTRA_H
#define DOM_MEDIA_PLATFORM_WMF_MFCDMEXTRA_H

// Currently, we build with WINVER=0x601 (Win7), which means the declarations in
// mfcontentdecryptionmodule.h will not be visible, which is only available on
// Win10 (0x0A00). Also, we don't yet have the Fall Creators Update SDK
// available on build machines, so even with updated WINVER, some of the
// interfaces we need would not be present. To work around this, until the build
// environment is updated, we include copies of the relevant classes/interfaces
// we need.
#if defined(WINVER) && WINVER >= 0x0A00
#  include <mfcontentdecryptionmodule.h>
#else
// For `IMFCdmSuspendNotify`
#  include "MFMediaEngineExtra.h"

typedef enum MF_MEDIAKEYSESSION_TYPE {
  MF_MEDIAKEYSESSION_TYPE_TEMPORARY = 0,
  MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE =
      (MF_MEDIAKEYSESSION_TYPE_TEMPORARY + 1),
  MF_MEDIAKEYSESSION_TYPE_PERSISTENT_RELEASE_MESSAGE =
      (MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE + 1),
  MF_MEDIAKEYSESSION_TYPE_PERSISTENT_USAGE_RECORD =
      (MF_MEDIAKEYSESSION_TYPE_PERSISTENT_RELEASE_MESSAGE + 1)
} MF_MEDIAKEYSESSION_TYPE;

typedef enum MF_MEDIAKEYSESSION_MESSAGETYPE {
  MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST = 0,
  MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RENEWAL = 1,
  MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE = 2,
  MF_MEDIAKEYSESSION_MESSAGETYPE_INDIVIDUALIZATION_REQUEST = 3
} MF_MEDIAKEYSESSION_MESSAGETYPE;

typedef enum MF_MEDIAKEY_STATUS {
  MF_MEDIAKEY_STATUS_USABLE = 0,
  MF_MEDIAKEY_STATUS_EXPIRED = (MF_MEDIAKEY_STATUS_USABLE + 1),
  MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED = (MF_MEDIAKEY_STATUS_EXPIRED + 1),
  MF_MEDIAKEY_STATUS_OUTPUT_NOT_ALLOWED =
      (MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED + 1),
  MF_MEDIAKEY_STATUS_STATUS_PENDING =
      (MF_MEDIAKEY_STATUS_OUTPUT_NOT_ALLOWED + 1),
  MF_MEDIAKEY_STATUS_INTERNAL_ERROR = (MF_MEDIAKEY_STATUS_STATUS_PENDING + 1),
  MF_MEDIAKEY_STATUS_RELEASED = (MF_MEDIAKEY_STATUS_INTERNAL_ERROR + 1),
  MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED = (MF_MEDIAKEY_STATUS_RELEASED + 1)
} MF_MEDIAKEY_STATUS;

typedef struct MFMediaKeyStatus {
  BYTE* pbKeyId;
  UINT cbKeyId;
  MF_MEDIAKEY_STATUS eMediaKeyStatus;
} MFMediaKeyStatus;

EXTERN_GUID(MF_CONTENTDECRYPTIONMODULE_SERVICE, 0x15320c45, 0xff80, 0x484a,
            0x9d, 0xcb, 0xd, 0xf8, 0x94, 0xe6, 0x9a, 0x1);

#  ifndef __IMFContentDecryptionModuleSessionCallbacks_INTERFACE_DEFINED__
#    define __IMFContentDecryptionModuleSessionCallbacks_INTERFACE_DEFINED__

/* interface IMFContentDecryptionModuleSessionCallbacks */
/* [unique][uuid][object] */

EXTERN_C const IID IID_IMFContentDecryptionModuleSessionCallbacks;

MIDL_INTERFACE("3f96ee40-ad81-4096-8470-59a4b770f89a")
IMFContentDecryptionModuleSessionCallbacks : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE KeyMessage(
      /* [in] */ MF_MEDIAKEYSESSION_MESSAGETYPE messageType,
      /* [size_is][in] */
      __RPC__in_ecount_full(messageSize) const BYTE* message,
      /* [in] */ DWORD messageSize,
      /* [optional][in] */ __RPC__in LPCWSTR destinationURL) = 0;

  virtual HRESULT STDMETHODCALLTYPE KeyStatusChanged(void) = 0;
};

#  endif /* __IMFContentDecryptionModuleSessionCallbacks_INTERFACE_DEFINED__ \
          */

#  ifndef __IMFContentDecryptionModuleSession_INTERFACE_DEFINED__
#    define __IMFContentDecryptionModuleSession_INTERFACE_DEFINED__

/* interface IMFContentDecryptionModuleSession */
/* [unique][uuid][object] */

EXTERN_C const IID IID_IMFContentDecryptionModuleSession;

MIDL_INTERFACE("4e233efd-1dd2-49e8-b577-d63eee4c0d33")
IMFContentDecryptionModuleSession : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetSessionId(
      /* [out] */ __RPC__deref_out_opt LPWSTR * sessionId) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetExpiration(
      /* [out] */ __RPC__out double* expiration) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetKeyStatuses(
      /* [size_is][size_is][out] */ __RPC__deref_out_ecount_full_opt(
          *numKeyStatuses) MFMediaKeyStatus *
          *keyStatuses,
      /* [out] */ __RPC__out UINT * numKeyStatuses) = 0;

  virtual HRESULT STDMETHODCALLTYPE Load(
      /* [in] */ __RPC__in LPCWSTR sessionId,
      /* [out] */ __RPC__out BOOL * loaded) = 0;

  virtual HRESULT STDMETHODCALLTYPE GenerateRequest(
      /* [in] */ __RPC__in LPCWSTR initDataType,
      /* [size_is][in] */
      __RPC__in_ecount_full(initDataSize) const BYTE* initData,
      /* [in] */ DWORD initDataSize) = 0;

  virtual HRESULT STDMETHODCALLTYPE Update(
      /* [size_is][in] */ __RPC__in_ecount_full(responseSize)
          const BYTE* response,
      /* [in] */ DWORD responseSize) = 0;

  virtual HRESULT STDMETHODCALLTYPE Close(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE Remove(void) = 0;
};

#  endif /* __IMFContentDecryptionModuleSession_INTERFACE_DEFINED__ */

#  ifndef __IMFContentDecryptionModule_INTERFACE_DEFINED__
#    define __IMFContentDecryptionModule_INTERFACE_DEFINED__

/* interface IMFContentDecryptionModule */
/* [unique][uuid][object] */

EXTERN_C const IID IID_IMFContentDecryptionModule;

MIDL_INTERFACE("87be986c-10be-4943-bf48-4b54ce1983a2")
IMFContentDecryptionModule : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE SetContentEnabler(
      /* [in] */ __RPC__in_opt IMFContentEnabler * contentEnabler,
      /* [in] */ __RPC__in_opt IMFAsyncResult * result) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetSuspendNotify(
      /* [out] */ __RPC__deref_out_opt IMFCdmSuspendNotify * *notify) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetPMPHostApp(
      /* [in] */ __RPC__in_opt IMFPMPHostApp * pmpHostApp) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateSession(
      /* [in] */ MF_MEDIAKEYSESSION_TYPE sessionType,
      /* [in] */ __RPC__in_opt IMFContentDecryptionModuleSessionCallbacks *
          callbacks,
      /* [out] */ __RPC__deref_out_opt IMFContentDecryptionModuleSession *
          *session) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetServerCertificate(
      /* [size_is][in] */ __RPC__in_ecount_full(certificateSize)
          const BYTE* certificate,
      /* [in] */ DWORD certificateSize) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateTrustedInput(
      /* [size_is][in] */ __RPC__in_ecount_full(contentInitDataSize)
          const BYTE* contentInitData,
      /* [in] */ DWORD contentInitDataSize,
      /* [out] */ __RPC__deref_out_opt IMFTrustedInput** trustedInput) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetProtectionSystemIds(
      /* [size_is][size_is][out] */ __RPC__deref_out_ecount_full_opt(*count)
              GUID *
          *systemIds,
      /* [out] */ __RPC__out DWORD * count) = 0;
};

#  endif /* __IMFContentDecryptionModule_INTERFACE_DEFINED__ */

#endif  //  defined(WINVER) && WINVER >= 0x0A00

#endif  // DOM_MEDIA_PLATFORM_WMF_MFCDMEXTRA_H
