/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _AccessibleWrap_H_
#define _AccessibleWrap_H_

#include "nsCOMPtr.h"
#include "Accessible.h"
#include "Accessible2.h"
#include "ia2AccessibleComponent.h"
#include "CAccessibleHyperlink.h"
#include "CAccessibleValue.h"

#define DECL_IUNKNOWN_INHERITED                                               \
public:                                                                       \
STDMETHODIMP QueryInterface(REFIID, void**);                                  \

#define IMPL_IUNKNOWN_QUERY_HEAD(Class)                                       \
STDMETHODIMP                                                                  \
Class::QueryInterface(REFIID iid, void** ppv)                                 \
{                                                                             \
  HRESULT hr = E_NOINTERFACE;                                                 \
  *ppv = NULL;                                                                \

#define IMPL_IUNKNOWN_QUERY_TAIL                                              \
  return hr;                                                                  \
}                                                                             \

#define IMPL_IUNKNOWN_QUERY_ENTRY(Class)                                      \
  hr = Class::QueryInterface(iid, ppv);                                       \
  if (SUCCEEDED(hr))                                                          \
    return hr;                                                                \

#define IMPL_IUNKNOWN_QUERY_ENTRY_COND(Class, Cond)                           \
  if (Cond) {                                                                 \
    hr = Class::QueryInterface(iid, ppv);                                     \
    if (SUCCEEDED(hr))                                                        \
      return hr;                                                              \
  }                                                                           \

#define IMPL_IUNKNOWN_INHERITED0(Class, Super)                                \
  IMPL_IUNKNOWN_QUERY_HEAD(Class)                                             \
  IMPL_IUNKNOWN_QUERY_ENTRY(Super)                                            \
  IMPL_IUNKNOWN_QUERY_TAIL                                                    \

#define IMPL_IUNKNOWN_INHERITED1(Class, Super, I1)                            \
  IMPL_IUNKNOWN_QUERY_HEAD(Class)                                             \
  IMPL_IUNKNOWN_QUERY_ENTRY(I1);                                              \
  IMPL_IUNKNOWN_QUERY_ENTRY(Super)                                            \
  IMPL_IUNKNOWN_QUERY_TAIL                                                    \

#define IMPL_IUNKNOWN_INHERITED2(Class, Super, I1, I2)                        \
  IMPL_IUNKNOWN_QUERY_HEAD(Class)                                             \
  IMPL_IUNKNOWN_QUERY_ENTRY(I1);                                              \
  IMPL_IUNKNOWN_QUERY_ENTRY(I2);                                              \
  IMPL_IUNKNOWN_QUERY_ENTRY(Super)                                            \
  IMPL_IUNKNOWN_QUERY_TAIL                                                    \


class AccessibleWrap : public Accessible,
                       public ia2AccessibleComponent,
                       public CAccessibleHyperlink,
                       public CAccessibleValue,
                       public IAccessible2
{
public: // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    Accessible(aContent, aDoc) { }
  virtual ~AccessibleWrap() { }

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

  public: // IAccessible2
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nRelations(
        /* [retval][out] */ long *nRelations);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relation(
        /* [in] */ long relationIndex,
        /* [retval][out] */ IAccessibleRelation **relation);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relations(
        /* [in] */ long maxRelations,
        /* [length_is][size_is][out] */ IAccessibleRelation **relation,
        /* [retval][out] */ long *nRelations);

    virtual HRESULT STDMETHODCALLTYPE role(
            /* [retval][out] */ long *role);

    virtual HRESULT STDMETHODCALLTYPE scrollTo(
        /* [in] */ enum IA2ScrollType scrollType);

    virtual HRESULT STDMETHODCALLTYPE scrollToPoint(
        /* [in] */ enum IA2CoordinateType coordinateType,
	      /* [in] */ long x,
	      /* [in] */ long y);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_groupPosition(
        /* [out] */ long *groupLevel,
        /* [out] */ long *similarItemsInGroup,
        /* [retval][out] */ long *positionInGroup);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_states(
        /* [retval][out] */ AccessibleStates *states);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_extendedRole(
        /* [retval][out] */ BSTR *extendedRole);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedExtendedRole(
        /* [retval][out] */ BSTR *localizedExtendedRole);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nExtendedStates(
        /* [retval][out] */ long *nExtendedStates);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_extendedStates(
        /* [in] */ long maxExtendedStates,
        /* [length_is][length_is][size_is][size_is][out] */ BSTR **extendedStates,
        /* [retval][out] */ long *nExtendedStates);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedExtendedStates(
        /* [in] */ long maxLocalizedExtendedStates,
        /* [length_is][length_is][size_is][size_is][out] */ BSTR **localizedExtendedStates,
        /* [retval][out] */ long *nLocalizedExtendedStates);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_uniqueID(
        /* [retval][out] */ long *uniqueID);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_windowHandle(
        /* [retval][out] */ HWND *windowHandle);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_indexInParent(
        /* [retval][out] */ long *indexInParent);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_locale(
        /* [retval][out] */ IA2Locale *locale);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_attributes(
        /* [retval][out] */ BSTR *attributes);

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

  // Helper methods
  static PRInt32 GetChildIDFor(Accessible* aAccessible);
  static HWND GetHWNDFor(Accessible* aAccessible);
  static HRESULT ConvertToIA2Attributes(nsIPersistentProperties *aAttributes,
                                        BSTR *aIA2Attributes);

  /**
   * System caret support: update the Windows caret position. 
   * The system caret works more universally than the MSAA caret
   * For example, Window-Eyes, JAWS, ZoomText and Windows Tablet Edition use it
   * We will use an invisible system caret.
   * Gecko is still responsible for drawing its own caret
   */
  void UpdateSystemCaret();

  /**
   * Find an accessible by the given child ID in cached documents.
   */
  Accessible* GetXPAccessibleFor(const VARIANT& aVarChild);

  NS_IMETHOD GetNativeInterface(void **aOutAccessible);

  static IDispatch *NativeAccessible(nsIAccessible *aXPAccessible);

protected:
  virtual nsresult FirePlatformEvent(AccEvent* aEvent);

  /**
   * Creates ITypeInfo for LIBID_Accessibility if it's needed and returns it.
   */
  static ITypeInfo* GetTI(LCID lcid);

  static ITypeInfo* gTypeInfo;


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
    NAVRELATION_DESCRIPTION_FOR = 0x100f
  };
};

// Define unsupported wrap classes here
typedef class nsHTMLTextFieldAccessible    nsHTMLTextFieldAccessibleWrap;
typedef class nsXULTextFieldAccessible     nsXULTextFieldAccessibleWrap;

#endif
