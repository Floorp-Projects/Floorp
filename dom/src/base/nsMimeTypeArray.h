/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMimeTypeArray_h___
#define nsMimeTypeArray_h___

#include "nsIDOMMimeTypeArray.h"
#include "nsIDOMMimeType.h"

class nsIDOMNavigator;

class MimeTypeArrayImpl : public nsIDOMMimeTypeArray
{
public:
	MimeTypeArrayImpl(nsIDOMNavigator* navigator);
	virtual ~MimeTypeArrayImpl();

	NS_DECL_ISUPPORTS

	NS_IMETHOD GetLength(PRUint32* aLength);
	NS_IMETHOD Item(PRUint32 aIndex, nsIDOMMimeType** aReturn);
	NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMMimeType** aReturn);
	nsresult   Refresh();

private:
  nsresult GetMimeTypes();
  void     Clear();

protected:
	nsIDOMNavigator* mNavigator;
	PRUint32 mMimeTypeCount;
	nsIDOMMimeType** mMimeTypeArray;
};

class MimeTypeElementImpl : public nsIDOMMimeType
{
public:
	MimeTypeElementImpl(nsIDOMPlugin* aPlugin, nsIDOMMimeType* aMimeType);
	virtual ~MimeTypeElementImpl();

	NS_DECL_ISUPPORTS

	NS_IMETHOD GetDescription(nsAWritableString& aDescription);
	NS_IMETHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin);
	NS_IMETHOD GetSuffixes(nsAWritableString& aSuffixes);
	NS_IMETHOD GetType(nsAWritableString& aType);

protected:
	nsIDOMPlugin* mPlugin;
	nsIDOMMimeType* mMimeType;
};

#endif /* nsMimeTypeArray_h___ */
