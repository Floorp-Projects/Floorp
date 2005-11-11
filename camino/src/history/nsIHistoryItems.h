/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Simon Fraser <smfr@smfr.org>
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

#ifndef nsihistoryitems_h__
#define nsihistoryitems_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

// 5D46D8CA-5BB2-11D9-A79E-000393D7254A
#define NS_IHISTORYITEM_IID_STR "5d46d8ca-5bb2-11d9-a79e-000393d7254a"

#define NS_IHISTORYITEM_IID \
  {0x5d46d8ca, 0x5bb2, 0x11d9, \
    { 0xa7, 0x9e, 0x00, 0x03, 0x39, 0xd7, 0x25, 0x4a }}

// 06A993AC-5BB0-11D9-85E7-000393D7254A
#define NS_IHISTORYITEMS_IID_STR "06a993ac-5bb0-11d9-85e7-000393d7254a"

#define NS_IHISTORYITEMS_IID \
  {0x06a993ac, 0x5bb0, 0x11d9, \
    { 0x85, 0xe7, 0x00, 0x03, 0x39, 0xd7, 0x25, 0x4a }}


// 
// nsIHistoryItem is used to communicate details about history
// items to clients.
// 
// Instances of nsIHistoryItem are _not_ persistent, so you cannot
// rely on getting the same nsIHistoryItem back from the enumerator
// a second time. Use the unique identifiers for this purpose.
// 
// (This saves us from having to maintain a collection of nsIHistoryItem
// objects that do little more than reflect the db row objects, but we
// should probably do this to remove the burdern from the client.)
//

class NS_NO_VTABLE nsIHistoryItem : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHISTORYITEM_IID)

  NS_IMETHOD GetURL(nsACString& outURL) = 0;
  NS_IMETHOD GetReferrer(nsACString& outReferrer) = 0;

  NS_IMETHOD GetLastVisitDate(PRTime* outLastVisit) = 0;
  NS_IMETHOD GetFirstVisitDate(PRTime* outFirstVisit) = 0;
  NS_IMETHOD GetVisitCount(PRInt32* outVisitCount) = 0;

  NS_IMETHOD GetTitle(nsAString& outURL) = 0;
  NS_IMETHOD GetHostname(nsACString& outURL) = 0;

  NS_IMETHOD GetHidden(PRBool* outHidden) = 0;
  NS_IMETHOD GetTyped(PRBool* outTyped) = 0;

  // return a unique, persistent (within sessions) ID for this item
  NS_IMETHOD GetID(nsACString& outIDString) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHistoryItem, NS_IHISTORYITEM_IID)

#define NS_DECL_NSIHISTORYITEM \
  NS_IMETHOD GetURL(nsACString& outURL); \
  NS_IMETHOD GetReferrer(nsACString& outReferrer); \
  NS_IMETHOD GetLastVisitDate(PRTime* outLastVisit); \
  NS_IMETHOD GetFirstVisitDate(PRTime* outFirstVisit); \
  NS_IMETHOD GetVisitCount(PRInt32* outVisitCount); \
  NS_IMETHOD GetTitle(nsAString& outURL); \
  NS_IMETHOD GetHostname(nsACString& outURL); \
  NS_IMETHOD GetHidden(PRBool* outHidden); \
  NS_IMETHOD GetTyped(PRBool* outTyped); \
  NS_IMETHOD GetID(nsACString& outIDString);

class nsISimpleEnumerator;
class nsIHistoryObserver;

class NS_NO_VTABLE nsIHistoryItems : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHISTORYITEMS_IID)

  /**
     * GetMaxItemCount
     * Get the max number of entries in global history (the real number
     * may be less because of hidden rows).
     */
  NS_IMETHOD GetMaxItemCount(PRUint32 *outCount) = 0;

  /**
     * GetItemEnumerator
     * Get an enumerator for all the items in global history. 
     * Enumerator items are instances of nsIHistoryItem
     */
  NS_IMETHOD GetItemEnumerator(nsISimpleEnumerator** outEnumerator) = 0;
  
  /**
     * Flush
     * Save the history database
     */
  NS_IMETHOD Flush() = 0;

  /**
     * AddObserver
     */
  NS_IMETHOD AddObserver(nsIHistoryObserver* inObserver) = 0;

  /**
     * RemoveObserver
     */
  NS_IMETHOD RemoveObserver(nsIHistoryObserver* inObserver) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHistoryItems, NS_IHISTORYITEMS_IID)

#define NS_DECL_NSIHISTORYITEMS \
  NS_IMETHOD GetMaxItemCount(PRUint32 *outCount); \
  NS_IMETHOD GetItemEnumerator(nsISimpleEnumerator** outEnumerator); \
  NS_IMETHOD Flush(); \
  NS_IMETHOD AddObserver(nsIHistoryObserver* inObserver); \
  NS_IMETHOD RemoveObserver(nsIHistoryObserver* inObserver);


#endif // nsihistoryitems_h__
