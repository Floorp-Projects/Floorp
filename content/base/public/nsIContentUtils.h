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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIContentUtils_h__
#define nsIContentUtils_h__

#include "nsIDocumentLoaderFactory.h"
#include "nsCOMPtr.h"
#include "nsAString.h"
#include "nsMargin.h"

class nsIInterfaceRequestor;

// {3682DD99-8560-44f4-9B8F-CCCE9D7B96FB}
#define NS_ICONTENTUTILS_IID \
{ 0x3682dd99, 0x8560, 0x44f4, \
  { 0x9b, 0x8f, 0xcc, 0xce, 0x9d, 0x7b, 0x96, 0xfb } }

class nsIContentUtils : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTUTILS_IID)
    NS_DECL_ISUPPORTS

    virtual PRBool IsSafeToRunScript();
    virtual PRBool ParseIntMarginValue(const nsAString& aString, nsIntMargin& result);

    enum ContentViewerType
    {
        TYPE_UNSUPPORTED,
        TYPE_CONTENT,
        TYPE_PLUGIN,
        TYPE_UNKNOWN
    };

    virtual already_AddRefed<nsIDocumentLoaderFactory>
    FindInternalContentViewer(const char* aType,
                              ContentViewerType* aLoaderType = NULL);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentUtils, NS_ICONTENTUTILS_IID)

// {c7193287-3e3d-467f-b6da-47b914eb4c83}
#define NS_ICONTENTUTILS2_IID \
{ 0xc7193287, 0x3e3d, 0x467f, \
{ 0xb6, 0xda, 0x47, 0xb9, 0x14, 0xeb, 0x4c, 0x83 } }

class nsIContentUtils2 : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTUTILS2_IID)
  NS_DECL_ISUPPORTS

  virtual nsIInterfaceRequestor* GetSameOriginChecker();
  // Returns NS_OK for same origin, error (NS_ERROR_DOM_BAD_URI) if not.
  virtual nsresult CheckSameOrigin(nsIChannel *aOldChannel, nsIChannel *aNewChannel);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentUtils2, NS_ICONTENTUTILS2_IID)

#ifndef MOZ_ENABLE_LIBXUL
// nsIContentUtils_MOZILLA_2_0_BRANCH is a non-libxul only interface to enable
// us keep those builds working.

#define NS_ICONTENTUTILS_MOZILLA_2_0_BRANCH_IID \
{ 0x0fe8099c, 0x622a, 0x4c79, \
{ 0xb0, 0x02, 0x55, 0xf0, 0x44, 0x34, 0x00, 0x30 } }

class nsIContentUtils_MOZILLA_2_0_BRANCH : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTURILS_MOZILLA_2_0_BRANCH_IID)
  NS_DECL_ISUPPORTS

  virtual nsresult DispatchTrustedEvent(nsIDocument* aDoc,
                                        nsISupports* aTarget,
                                        const nsAString& aEventName,
                                        PRBool aCanBubble,
                                        PRBool aCancelable,
                                        PRBool *aDefaultAction = nsnull);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentUtils_MOZILLA_2_0_BRANCH, NS_ICONTENTUTILS_MOZILLA_2_0_BRANCH_IID)

#endif

#endif /* nsIContentUtils_h__ */
