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

#ifndef nsihistoryobserver_h__
#define nsihistoryobserver_h__

#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

#include "nsIHistoryItems.h"

// CB1059D7-5BC5-11D9-8254-000393D7254A
#define NS_IHISTORYOBSERVER_IID_STR "cb1059d7-5bc5-11d9-8254-000393d7254a"

#define NS_IHISTORYOBSERVER_IID \
  {0xcb1059d7, 0x5bc5, 0x11d9, \
    { 0x82, 0x54, 0x00, 0x03, 0x39, 0xd7, 0x25, 0x4a }}

class nsIHistoryObserver : public nsISupports
{
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHISTORYOBSERVER_IID)

  NS_IMETHOD HistoryLoaded() = 0;

  NS_IMETHOD ItemLoaded(nsIHistoryItem* inHistoryItem, PRBool inFirstVisit) = 0;

  NS_IMETHOD ItemRemoved(nsIHistoryItem* inHistoryItem) = 0;

  NS_IMETHOD ItemTitleChanged(nsIHistoryItem* inHistoryItem) = 0;

  NS_IMETHOD  StartBatchChanges() = 0;
  NS_IMETHOD  EndBatchChanges() = 0;
  
};


#define NS_DECL_NSIHISTORYOBSERVER \
  NS_IMETHOD HistoryLoaded(); \
  NS_IMETHOD ItemLoaded(nsIHistoryItem* inHistoryItem, PRBool inFirstVisit); \
  NS_IMETHOD ItemRemoved(nsIHistoryItem* inHistoryItem); \
  NS_IMETHOD ItemTitleChanged(nsIHistoryItem* inHistoryItem); \
  NS_IMETHOD  StartBatchChanges(); \
  NS_IMETHOD  EndBatchChanges();

#endif // nsihistoryobserver_h__
