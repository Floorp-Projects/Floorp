/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _nsIMsgFilterService_H_
#define _nsIMsgFilterService_H_


#include "nsISupports.h"

//6673cad0-072e-11d3-8d70-00805f8a6617
#define NS_IMSGFILTERSERVICE_IID                         \
{ 0x6673cad0, 0x072e, 0x11d3,                  \
    { 0x8d, 0x70, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x17 } }


class nsIMsgFilterList;

class nsIMsgFilterService : public nsISupports
{

public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTERSERVICE_IID; return iid; }

/* clients call OpenFilterList to get a handle to a FilterList, of existing nsMsgFilter *.
	These are manipulated by the front end as a result of user interaction
   with dialog boxes. To apply the new list call MSG_CloseFilterList.

*/
	NS_IMETHOD OpenFilterList(nsFileSpec *filterFile, nsIMsgFilterList **filterList) = 0;
	NS_IMETHOD CloseFilterList(nsIMsgFilterList *filterList) = 0;
	NS_IMETHOD	SaveFilterList(nsIMsgFilterList *filterList, nsFileSpec *filterFile) = 0;	/* save without deleting */
	NS_IMETHOD CancelFilterList(nsIMsgFilterList *filterList) = 0;

};

#endif  // _nsIMsgFilterService_H_

