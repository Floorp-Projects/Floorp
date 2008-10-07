/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com>
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

#ifndef nsIXULPrototypeCache_h__
#define nsIXULPrototypeCache_h__

#include "nsISupports.h"
class nsIURI;

// {3A0A0FC1-8349-11d3-BE47-00104BDE6048}
#define NS_XULPROTOTYPECACHE_CID \
{ 0x3a0a0fc1, 0x8349, 0x11d3, { 0xbe, 0x47, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

// {f8bee3d7-4be8-46ae-92c2-60c25d5cd647}
#define NS_IXULPROTOTYPECACHE_IID \
{ 0xf8bee3d7, 0x4be8, 0x46ae, \
  { 0x92, 0xc2, 0x60, 0xc2, 0x5d, 0x5c, 0xd6, 0x47 } }

/**
 * This interface lets code from outside gklayout access the prototype cache.
 */
class nsIXULPrototypeCache : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXULPROTOTYPECACHE_IID)

    /**
     * Whether the XUL document at the specified URI is in the cache.
     */
    virtual PRBool IsCached(nsIURI* aURI) = 0;

    /**
     * Stop the FastLoad process abruptly, removing the FastLoad file.
     */
    virtual void AbortFastLoads() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXULPrototypeCache, NS_IXULPROTOTYPECACHE_IID)

NS_IMETHODIMP
NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);


const char XUL_FASTLOAD_FILE_BASENAME[] = "XUL";

// Increase the subtractor when changing version, say when changing the
// (opaque to XPCOM FastLoad code) format of XUL-specific XDR serializations.
// See also JSXDR_BYTECODE_VERSION in jsxdrapi.h, which tracks incompatible JS
// bytecode version changes.
#define XUL_FASTLOAD_FILE_VERSION       (0xfeedbeef - 25)

#define XUL_SERIALIZATION_BUFFER_SIZE   (64 * 1024)
#define XUL_DESERIALIZATION_BUFFER_SIZE (8 * 1024)


#endif // nsIXULPrototypeCache_h__
