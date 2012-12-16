/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_sdnAccessible_h_
#define mozilla_a11y_sdnAccessible_h_

#include "ISimpleDOMNode.h"
#include "AccessibleWrap.h"

#include "mozilla/Attributes.h"

namespace mozilla {
namespace a11y {

class sdnAccessible MOZ_FINAL : public ISimpleDOMNode
{
public:
  sdnAccessible(nsINode* aNode) : mNode(aNode) { }
  ~sdnAccessible() { }

  /**
   * Retrun if the object is defunct.
   */
  bool IsDefunct() const { return !GetDocument(); }

  /**
   * Return a document accessible it belongs to if any.
   */
  DocAccessible* GetDocument() const;

  /*
   * Return associated accessible if any.
   */
  Accessible* GetAccessible() const;

  //IUnknown
  DECL_IUNKNOWN

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeInfo(
    /* [out] */ BSTR __RPC_FAR* aNodeName,
    /* [out] */ short __RPC_FAR* aNameSpaceID,
    /* [out] */ BSTR __RPC_FAR* aNodeValue,
    /* [out] */ unsigned int __RPC_FAR* aNumChildren,
    /* [out] */ unsigned int __RPC_FAR* aUniqueID,
    /* [out][retval] */ unsigned short __RPC_FAR* aNodeType);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_attributes(
    /* [in] */ unsigned short aMaxAttribs,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR* aAttribNames,
    /* [length_is][size_is][out] */ short __RPC_FAR* aNameSpaceIDs,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR* aAttribValues,
    /* [out][retval] */ unsigned short __RPC_FAR* aNumAttribs);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_attributesForNames(
    /* [in] */ unsigned short aMaxAttribs,
    /* [length_is][size_is][in] */ BSTR __RPC_FAR* aAttribNames,
    /* [length_is][size_is][in] */ short __RPC_FAR* aNameSpaceID,
    /* [length_is][size_is][retval] */ BSTR __RPC_FAR* aAttribValues);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_computedStyle(
    /* [in] */ unsigned short aMaxStyleProperties,
    /* [in] */ boolean aUseAlternateView,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR* aStyleProperties,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR* aStyleValues,
    /* [out][retval] */ unsigned short __RPC_FAR* aNumStyleProperties);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_computedStyleForProperties(
    /* [in] */ unsigned short aNumStyleProperties,
    /* [in] */ boolean aUseAlternateView,
    /* [length_is][size_is][in] */ BSTR __RPC_FAR* aStyleProperties,
    /* [length_is][size_is][out][retval] */ BSTR __RPC_FAR* aStyleValues);

  virtual HRESULT STDMETHODCALLTYPE scrollTo(/* [in] */ boolean aScrollTopLeft);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_parentNode(
    /* [out][retval] */ ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_firstChild(
    /* [out][retval] */ ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_lastChild(
    /* [out][retval] */ ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_previousSibling(
    /* [out][retval] */ ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nextSibling(
    /* [out][retval] */ ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_childAt(
    /* [in] */ unsigned aChildIndex,
    /* [out][retval] */ ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_innerHTML(
    /* [out][retval] */ BSTR __RPC_FAR* aInnerHTML);

  virtual /* [local][propget] */ HRESULT STDMETHODCALLTYPE get_localInterface(
    /* [retval][out] */ void __RPC_FAR *__RPC_FAR* aLocalInterface);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_language(
    /* [out][retval] */ BSTR __RPC_FAR* aLanguage);

private:
  nsCOMPtr<nsINode> mNode;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_sdnAccessible_h_
