/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentPermissionHelper_h
#define nsContentPermissionHelper_h

#include "nsIContentPermissionPrompt.h"
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "mozilla/dom/PContentPermissionRequestChild.h"
// Microsoft's API Name hackery sucks
// XXXbz Doing this in a header is a gigantic footgun. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=932421#c3 for why.
#undef LoadImage

class nsPIDOMWindow;
class nsContentPermissionRequestProxy;

// Forward declare IPC::Principal here which is defined in
// PermissionMessageUtils.h. Include this file will transitively includes
// "windows.h" and it defines
//   #define CreateEvent CreateEventW
//   #define LoadImage LoadImageW
// That will mess up windows build.
namespace IPC {
class Principal;
}

namespace mozilla {
namespace dom {

class Element;
class PermissionRequest;
class ContentPermissionRequestParent;
class PContentPermissionRequestParent;

class ContentPermissionType : public nsIContentPermissionType
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONTYPE

  ContentPermissionType(const nsACString& aType,
                        const nsACString& aAccess,
                        const nsTArray<nsString>& aOptions);

protected:
  virtual ~ContentPermissionType();

  nsCString mType;
  nsCString mAccess;
  nsTArray<nsString> mOptions;
};

class nsContentPermissionUtils
{
public:
  static uint32_t
  ConvertPermissionRequestToArray(nsTArray<PermissionRequest>& aSrcArray,
                                  nsIMutableArray* aDesArray);

  static uint32_t
  ConvertArrayToPermissionRequest(nsIArray* aSrcArray,
                                  nsTArray<PermissionRequest>& aDesArray);

  static nsresult
  CreatePermissionArray(const nsACString& aType,
                        const nsACString& aAccess,
                        const nsTArray<nsString>& aOptions,
                        nsIArray** aTypesArray);

  static PContentPermissionRequestParent*
  CreateContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                       Element* element,
                                       const IPC::Principal& principal);

  static nsresult
  AskPermission(nsIContentPermissionRequest* aRequest, nsPIDOMWindow* aWindow);
};

} // namespace dom
} // namespace mozilla

class nsContentPermissionRequestProxy : public nsIContentPermissionRequest
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  nsContentPermissionRequestProxy();

  nsresult Init(const nsTArray<mozilla::dom::PermissionRequest>& requests,
                mozilla::dom::ContentPermissionRequestParent* parent);

  void OnParentDestroyed();

 private:
  virtual ~nsContentPermissionRequestProxy();

  // Non-owning pointer to the ContentPermissionRequestParent object which owns this proxy.
  mozilla::dom::ContentPermissionRequestParent* mParent;
  nsTArray<mozilla::dom::PermissionRequest> mPermissionRequests;
};

/**
 * RemotePermissionRequest will send a prompt ipdl request to b2g process.
 */
class RemotePermissionRequest MOZ_FINAL : public nsISupports
                                        , public mozilla::dom::PContentPermissionRequestChild
{
public:
  NS_DECL_ISUPPORTS

  RemotePermissionRequest(nsIContentPermissionRequest* aRequest,
                          nsPIDOMWindow* aWindow);

  // It will be called when prompt dismissed.
  virtual bool Recv__delete__(const bool &aAllow,
                              const nsTArray<PermissionChoice>& aChoices) MOZ_OVERRIDE;

  void IPDLAddRef()
  {
    mIPCOpen = true;
    AddRef();
  }

  void IPDLRelease()
  {
    mIPCOpen = false;
    Release();
  }

private:
  virtual ~RemotePermissionRequest()
  {
    MOZ_ASSERT(!mIPCOpen, "Protocol must not be open when RemotePermissionRequest is destroyed.");
  }

  void DoAllow(JS::HandleValue aChoices);
  void DoCancel();

  nsCOMPtr<nsIContentPermissionRequest> mRequest;
  nsCOMPtr<nsPIDOMWindow>               mWindow;
  bool                                  mIPCOpen;
};

#endif // nsContentPermissionHelper_h
