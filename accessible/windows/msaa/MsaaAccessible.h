/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaAccessible_h_
#define mozilla_a11y_MsaaAccessible_h_

#include "ia2Accessible.h"
#include "ia2AccessibleComponent.h"
#include "ia2AccessibleHyperlink.h"
#include "ia2AccessibleValue.h"
#include "IUnknownImpl.h"
#include "mozilla/a11y/MsaaIdGenerator.h"
#include "mozilla/dom/ipc/IdType.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace a11y {
class Accessible;
class AccessibleWrap;
class LocalAccessible;
class sdnAccessible;

class MsaaAccessible : public ia2Accessible,
                       public ia2AccessibleComponent,
                       public ia2AccessibleHyperlink,
                       public ia2AccessibleValue {
 public:
  static MsaaAccessible* Create(Accessible* aAcc);

  Accessible* Acc() { return mAcc; }
  AccessibleWrap* LocalAcc();

  uint32_t GetExistingID() const { return mID; }
  static const uint32_t kNoID = 0;
  void SetID(uint32_t aID);

  static int32_t GetChildIDFor(Accessible* aAccessible);
  static uint32_t GetContentProcessIdFor(dom::ContentParentId aIPCContentId);
  static void ReleaseContentProcessIdFor(dom::ContentParentId aIPCContentId);
  static void AssignChildIDTo(NotNull<sdnAccessible*> aSdnAcc);
  static void ReleaseChildID(NotNull<sdnAccessible*> aSdnAcc);
  static HWND GetHWNDFor(Accessible* aAccessible);
  static void FireWinEvent(Accessible* aTarget, uint32_t aEventType);

  /**
   * Find an accessible by the given child ID in cached documents.
   */
  [[nodiscard]] already_AddRefed<IAccessible> GetIAccessibleFor(
      const VARIANT& aVarChild, bool* aIsDefunct);

  /**
   * Associate a COM object with this MsaaAccessible so it will be disconnected
   * from remote clients when this MsaaAccessible shuts down.
   * This should only be called with separate COM objects with a different
   * IUnknown to this MsaaAccessible; e.g. IAccessibleRelation.
   */
  void AssociateCOMObjectForDisconnection(IUnknown* aObject) {
    // We only need to track these for content processes because COM garbage
    // collection is disabled there.
    if (XRE_IsContentProcess()) {
      mAssociatedCOMObjectsForDisconnection.AppendElement(aObject);
    }
  }

  void MsaaShutdown();

  static IDispatch* NativeAccessible(Accessible* aAccessible);

  static MsaaAccessible* GetFrom(Accessible* aAcc);

  DECL_IUNKNOWN

  // IAccessible
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accParent(
      /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispParent)
      override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accChildCount(
      /* [retval][out] */ long __RPC_FAR* pcountChildren) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accChild(
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispChild) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszName) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszValue) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accDescription(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszDescription) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accRole(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR* pvarRole) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accState(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR* pvarState) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accHelp(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszHelp) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accHelpTopic(
      /* [out] */ BSTR __RPC_FAR* pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR* pidTopic) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszKeyboardShortcut) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accFocus(
      /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accSelection(
      /* [retval][out] */ VARIANT __RPC_FAR* pvarChildren) override;
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accDefaultAction(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszDefaultAction) override;
  virtual /* [id] */ HRESULT STDMETHODCALLTYPE accSelect(
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild) override;
  virtual /* [id] */ HRESULT STDMETHODCALLTYPE accLocation(
      /* [out] */ long __RPC_FAR* pxLeft,
      /* [out] */ long __RPC_FAR* pyTop,
      /* [out] */ long __RPC_FAR* pcxWidth,
      /* [out] */ long __RPC_FAR* pcyHeight,
      /* [optional][in] */ VARIANT varChild) override;
  virtual /* [id] */ HRESULT STDMETHODCALLTYPE accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR* pvarEndUpAt) override;
  virtual /* [id] */ HRESULT STDMETHODCALLTYPE accHitTest(
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) override;
  virtual /* [id] */ HRESULT STDMETHODCALLTYPE accDoDefaultAction(
      /* [optional][in] */ VARIANT varChild) override;
  virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szName) override;
  virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szValue) override;

  // IDispatch (support of scripting languages like VB)
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override;
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid,
                                                ITypeInfo** ppTInfo) override;
  virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid,
                                                  LPOLESTR* rgszNames,
                                                  UINT cNames, LCID lcid,
                                                  DISPID* rgDispId) override;
  virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid,
                                           LCID lcid, WORD wFlags,
                                           DISPPARAMS* pDispParams,
                                           VARIANT* pVarResult,
                                           EXCEPINFO* pExcepInfo,
                                           UINT* puArgErr) override;

 protected:
  explicit MsaaAccessible(Accessible* aAcc);
  virtual ~MsaaAccessible();

  Accessible* mAcc;

  uint32_t mID;
  static MsaaIdGenerator sIDGen;

  HRESULT
  ResolveChild(const VARIANT& aVarChild, IAccessible** aOutInterface);

  enum navRelations {
    NAVRELATION_CONTROLLED_BY = 0x1000,
    NAVRELATION_CONTROLLER_FOR = 0x1001,
    NAVRELATION_LABEL_FOR = 0x1002,
    NAVRELATION_LABELLED_BY = 0x1003,
    NAVRELATION_MEMBER_OF = 0x1004,
    NAVRELATION_NODE_CHILD_OF = 0x1005,
    NAVRELATION_FLOWS_TO = 0x1006,
    NAVRELATION_FLOWS_FROM = 0x1007,
    NAVRELATION_SUBWINDOW_OF = 0x1008,
    NAVRELATION_EMBEDS = 0x1009,
    NAVRELATION_EMBEDDED_BY = 0x100a,
    NAVRELATION_POPUP_FOR = 0x100b,
    NAVRELATION_PARENT_WINDOW_OF = 0x100c,
    NAVRELATION_DEFAULT_BUTTON = 0x100d,
    NAVRELATION_DESCRIBED_BY = 0x100e,
    NAVRELATION_DESCRIPTION_FOR = 0x100f,
    NAVRELATION_NODE_PARENT_OF = 0x1010,
    NAVRELATION_CONTAINING_DOCUMENT = 0x1011,
    NAVRELATION_CONTAINING_TAB_PANE = 0x1012,
    NAVRELATION_CONTAINING_WINDOW = 0x1013,
    NAVRELATION_CONTAINING_APPLICATION = 0x1014,
    NAVRELATION_DETAILS = 0x1015,
    NAVRELATION_DETAILS_FOR = 0x1016,
    NAVRELATION_ERROR = 0x1017,
    NAVRELATION_ERROR_FOR = 0x1018,
    NAVRELATION_LINKS_TO = 0x1019
  };

 private:
  /**
   * Find a remote accessible by the given child ID.
   */
  [[nodiscard]] already_AddRefed<IAccessible> GetRemoteIAccessibleFor(
      const VARIANT& aVarChild);

  nsTArray<RefPtr<IUnknown>> mAssociatedCOMObjectsForDisconnection;

  /**
   * Creates ITypeInfo for LIBID_Accessibility if it's needed and returns it.
   */
  static ITypeInfo* GetTI(LCID lcid);
  static ITypeInfo* gTypeInfo;
};

}  // namespace a11y
}  // namespace mozilla

#ifdef XP_WIN
// Undo the windows.h damage
#  undef GetMessage
#  undef CreateEvent
#  undef GetClassName
#  undef GetBinaryType
#  undef RemoveDirectory
#endif

#endif
