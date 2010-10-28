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
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsAccessibleWrap_H_
#define _nsAccessibleWrap_H_

#include "nsCOMPtr.h"
#include "nsAccessible.h"
#include "Accessible2.h"
#include "CAccessibleComponent.h"
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


class nsAccessibleWrap : public nsAccessible,
                         public CAccessibleComponent,
                         public CAccessibleHyperlink,
                         public CAccessibleValue,
                         public IAccessible2,
                         public IEnumVARIANT
{
public: // construction, destruction
  nsAccessibleWrap(nsIContent *aContent, nsIWeakReference *aShell);
  virtual ~nsAccessibleWrap();

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

  public: // IEnumVariant
    // If there are two clients using this at the same time, and they are
    // each using a different mEnumVariant position it would be bad, because
    // we have only 1 object and can only keep of mEnumVARIANT position once.

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Next( 
        /* [in] */ ULONG celt,
        /* [length_is][size_is][out] */ VARIANT __RPC_FAR *rgVar,
        /* [out] */ ULONG __RPC_FAR *pCeltFetched);
  
    virtual HRESULT STDMETHODCALLTYPE Skip( 
        /* [in] */ ULONG celt);
  
    virtual HRESULT STDMETHODCALLTYPE Reset( void);
  
    virtual HRESULT STDMETHODCALLTYPE Clone( 
        /* [out] */ IEnumVARIANT __RPC_FAR *__RPC_FAR *ppEnum);

        
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

  // nsAccessible
  virtual nsresult HandleAccEvent(AccEvent* aEvent);

  // Helper methods
  static PRInt32 GetChildIDFor(nsAccessible* aAccessible);
  static HWND GetHWNDFor(nsAccessible *aAccessible);
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
  virtual nsAccessible *GetXPAccessibleFor(const VARIANT& aVarChild);

  NS_IMETHOD GetNativeInterface(void **aOutAccessible);

  // NT4 does not have the oleacc that defines these methods. So we define copies here that automatically
  // load the library only if needed.
  static STDMETHODIMP AccessibleObjectFromWindow(HWND hwnd,DWORD dwObjectID,REFIID riid,void **ppvObject);
  static STDMETHODIMP NotifyWinEvent(DWORD event,HWND hwnd,LONG idObjectType,LONG idObject);

  static IDispatch *NativeAccessible(nsIAccessible *aXPAccessible);

  /**
   * Drops the IEnumVariant current position so that navigation methods
   * Next() and Skip() doesn't work until Reset() method is called. The method
   * is used when children of the accessible are changed.
   */
  void UnattachIEnumVariant();

protected:
  virtual nsresult FirePlatformEvent(AccEvent* aEvent);

  // mEnumVARIANTPosition not the current accessible's position, but a "cursor" of 
  // where we are in the current list of children, with respect to
  // nsIEnumVariant::Reset(), Skip() and Next().
  PRInt32 mEnumVARIANTPosition;

  /**
   * Creates ITypeInfo for LIBID_Accessibility if it's needed and returns it.
   */
  ITypeInfo *GetTI(LCID lcid);

  ITypeInfo *mTypeInfo;


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
