/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __nsMsgResultElement_h
#define __nsMsgResultElement_h

#include "nsMsgSearchCore.h"
#include "nsIMsgSearchAdapter.h"
#include "nsMsgSearchArray.h"

// nsMsgResultElement specifies a single search hit.

//---------------------------------------------------------------------------
// nsMsgResultElement is a list of attribute/value pairs which are used to
// represent a search hit without requiring a message header or server connection
//---------------------------------------------------------------------------

class nsMsgResultElement
{
public:
	nsMsgResultElement (nsIMsgSearchAdapter *);
	virtual ~nsMsgResultElement ();

	static nsresult AssignValues (nsIMsgSearchValue *src, nsMsgSearchValue *dst);
	nsresult GetValue (nsMsgSearchAttribValue, nsMsgSearchValue **) const;
	nsresult AddValue (nsIMsgSearchValue*);
    nsresult AddValue (nsMsgSearchValue*);

	nsresult GetPrettyName (nsMsgSearchValue**);


	nsresult GetValueRef (nsMsgSearchAttribValue, nsIMsgSearchValue**) const;
	nsresult Open (void *window);

	// added as part of the search as view capabilities...
	static int CompareByFolderInfoPtrs (const void *, const void *);  

	static int Compare (const void *, const void *);
	static nsresult DestroyValue (nsIMsgSearchValue *value);

    nsCOMPtr<nsISupportsArray> m_valueList;
	nsIMsgSearchAdapter *m_adapter;

protected:
};

#endif
