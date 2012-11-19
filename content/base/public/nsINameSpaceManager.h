/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINameSpaceManager_h___
#define nsINameSpaceManager_h___

#include "nsISupports.h"
#include "nsStringGlue.h"

class nsIAtom;
class nsString;

#define kNameSpaceID_Unknown -1
// 0 is special at C++, so use a static const int32_t for
// kNameSpaceID_None to keep if from being cast to pointers
// Note that the XBL cache assumes (and asserts) that it can treat a
// single-byte value higher than kNameSpaceID_LastBuiltin specially. 
static const int32_t kNameSpaceID_None = 0;
#define kNameSpaceID_XMLNS    1 // not really a namespace, but it needs to play the game
#define kNameSpaceID_XML      2
#define kNameSpaceID_XHTML    3
#define kNameSpaceID_XLink    4
#define kNameSpaceID_XSLT     5
#define kNameSpaceID_XBL      6
#define kNameSpaceID_MathML   7
#define kNameSpaceID_RDF      8
#define kNameSpaceID_XUL      9
#define kNameSpaceID_SVG      10
#define kNameSpaceID_LastBuiltin          10 // last 'built-in' namespace
 
#define NS_NAMESPACEMANAGER_CONTRACTID "@mozilla.org/content/namespacemanager;1"

#define NS_INAMESPACEMANAGER_IID \
  { 0xd74e83e6, 0xf932, 0x4289, \
    { 0xac, 0x95, 0x9e, 0x10, 0x24, 0x30, 0x88, 0xd6 } }

/**
 * The Name Space Manager tracks the association between a NameSpace
 * URI and the int32_t runtime id. Mappings between NameSpaces and 
 * NameSpace prefixes are managed by nsINameSpaces.
 *
 * All NameSpace URIs are stored in a global table so that IDs are
 * consistent accross the app. NameSpace IDs are only consistent at runtime
 * ie: they are not guaranteed to be consistent accross app sessions.
 *
 * The nsINameSpaceManager needs to have a live reference for as long as
 * the NameSpace IDs are needed.
 *
 */

class nsINameSpaceManager : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INAMESPACEMANAGER_IID)

  virtual nsresult RegisterNameSpace(const nsAString& aURI,
                                     int32_t& aNameSpaceID) = 0;

  virtual nsresult GetNameSpaceURI(int32_t aNameSpaceID, nsAString& aURI) = 0;
  virtual int32_t GetNameSpaceID(const nsAString& aURI) = 0;

  virtual bool HasElementCreator(int32_t aNameSpaceID) = 0;
};
 
NS_DEFINE_STATIC_IID_ACCESSOR(nsINameSpaceManager, NS_INAMESPACEMANAGER_IID)

nsresult NS_GetNameSpaceManager(nsINameSpaceManager** aInstancePtrResult);

void NS_NameSpaceManagerShutdown();

#endif // nsINameSpaceManager_h___
