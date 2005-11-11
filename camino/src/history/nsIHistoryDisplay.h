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


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif


// 06A993AC-5BB0-11D9-85E7-000393D7254A
#define NS_IHISTORYDISPLAY_IID_STR "06a993ac-5bb0-11d9-85e7-000393d7254a"

#define NS_IHISTORYDISPLAY_IID \
  {0x06a993ac, 0x5bb0, 0x11d9, \
    { 0x85, 0xe7, 0x00, 0x03, 0x39, 0xd7, 0x25, 0x4a }}


class NS_NO_VTABLE nsIHistoryDisplay : public nsISupports
{
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHISTORYDISPLAY_IID)


  /**
     * GetItemCount
     * Get the number of entries in global history
     */
  NS_IMETHOD GetItemCount(PRUint32 *outCount) = 0;

  /**
     * GetItemEnumerator
     * Get an enumerator for all the items in global history
     */
  NS_IMETHOD GetItemEnumerator(nsISimpleEnumerator** outEnumerator) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHistoryDisplay, NS_IHISTORYDISPLAY_IID)

#define NS_DECL_NSIHISTORYDISPLAY \
  NS_IMETHOD GetItemCount(PRUint32 *outCount); \
  NS_IMETHOD GetItemEnumerator(nsISimpleEnumerator** outEnumerator);
