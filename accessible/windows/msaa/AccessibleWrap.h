/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleWrap_h_
#define mozilla_a11y_AccessibleWrap_h_

#include "nsCOMPtr.h"
#include "Accessible.h"
#include "Accessible2.h"
#include "ia2Accessible.h"
#include "ia2AccessibleComponent.h"
#include "ia2AccessibleHyperlink.h"
#include "ia2AccessibleValue.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "mozilla/a11y/IDSet.h"

#ifdef __GNUC__
// Inheriting from both XPCOM and MSCOM interfaces causes a lot of warnings
// about virtual functions being hidden by each other. This is done by
// design, so silence the warning.
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

namespace mozilla {
namespace a11y {

class AccessibleWrap : public Accessible,
                       public ia2Accessible,
                       public ia2AccessibleComponent,
                       public ia2AccessibleHyperlink,
                       public ia2AccessibleValue
{
public: // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  public: // IUnknown methods - see iunknown.h for documentation
    STDMETHODIMP QueryInterface(REFIID, void**);

  // Return the registered OLE class ID of this object's CfDataObj.
    CLSID GetClassID() const;

  public: // COM interface IAccessible
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accParent( 
        /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispParent);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accChildCount( 
        /* [retval][out] */ long __RPC_FAR *pcountChildren);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accChild( 
        /* [in] */ VARIANT varChild,
        /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accName( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszName);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszValue);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accDescription( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszDescription);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accRole( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarRole);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accState( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarState);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accHelp( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszHelp);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accHelpTopic( 
        /* [out] */ BSTR __RPC_FAR *pszHelpFile,
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ long __RPC_FAR *pidTopic);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accFocus( 
        /* [retval][out] */ VARIANT __RPC_FAR *pvarChild);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accSelection( 
        /* [retval][out] */ VARIANT __RPC_FAR *pvarChildren);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accDefaultAction( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accSelect( 
        /* [in] */ long flagsSelect,
        /* [optional][in] */ VARIANT varChild);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accLocation( 
        /* [out] */ long __RPC_FAR *pxLeft,
        /* [out] */ long __RPC_FAR *pyTop,
        /* [out] */ long __RPC_FAR *pcxWidth,
        /* [out] */ long __RPC_FAR *pcyHeight,
        /* [optional][in] */ VARIANT varChild);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accNavigate( 
        /* [in] */ long navDir,
        /* [optional][in] */ VARIANT varStart,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accHitTest( 
        /* [in] */ long xLeft,
        /* [in] */ long yTop,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarChild);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accDoDefaultAction( 
        /* [optional][in] */ VARIANT varChild);

    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_accName( 
        /* [optional][in] */ VARIANT varChild,
        /* [in] */ BSTR szName);

    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_accValue( 
        /* [optional][in] */ VARIANT varChild,
        /* [in] */ BSTR szValue);

  // IDispatch (support of scripting languages like VB)
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo);

  virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid,
                                                ITypeInfo **ppTInfo);

  virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid,
                                                  LPOLESTR *rgszNames,
                                                  UINT cNames,
                                                  LCID lcid,
                                                  DISPID *rgDispId);

  virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid,
                                           LCID lcid, WORD wFlags,
                                           DISPPARAMS *pDispParams,
                                           VARIANT *pVarResult,
                                           EXCEPINFO *pExcepInfo,
                                           UINT *puArgErr);

  // Accessible
  virtual nsresult HandleAccEvent(AccEvent* aEvent);
  virtual void Shutdown() override;

  // Helper methods
  static int32_t GetChildIDFor(Accessible* aAccessible);
  static HWND GetHWNDFor(Accessible* aAccessible);

  /**
   * System caret support: update the Windows caret position. 
   * The system caret works more universally than the MSAA caret
   * For example, Window-Eyes, JAWS, ZoomText and Windows Tablet Edition use it
   * We will use an invisible system caret.
   * Gecko is still responsible for drawing its own caret
   */
  void UpdateSystemCaretFor(Accessible* aAccessible);

  /**
   * Find an accessible by the given child ID in cached documents.
   */
  Accessible* GetXPAccessibleFor(const VARIANT& aVarChild);

  virtual void GetNativeInterface(void **aOutAccessible) override;

  static IDispatch* NativeAccessible(Accessible* aAccessible);

protected:
  virtual ~AccessibleWrap();

#ifdef _WIN64
  uint32_t mID;
#endif

  /**
   * Creates ITypeInfo for LIBID_Accessibility if it's needed and returns it.
   */
  static ITypeInfo* GetTI(LCID lcid);

  static ITypeInfo* gTypeInfo;

#ifdef _WIN64
  static IDSet sIDGen;
#endif

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
    NAVRELATION_CONTAINING_APPLICATION = 0x1014
  };
};

static inline AccessibleWrap*
WrapperFor(ProxyAccessible* aProxy)
{
  return reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());
}
} // namespace a11y
} // namespace mozilla

#ifdef XP_WIN
// Undo the windows.h damage
#undef GetMessage
#undef CreateEvent
#undef GetClassName
#undef GetBinaryType
#undef RemoveDirectory
#endif

#endif
