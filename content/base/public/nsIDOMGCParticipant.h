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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
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

#ifndef nsIDOMGCParticipant_h_
#define nsIDOMGCParticipant_h_

#include "nscore.h"
#include "nsISupports.h"
#include "nsCOMArray.h"

// 0e2a5a8d-28fd-4a5c-8bf1-5b0067ff3286
#define NS_IDOMGCPARTICIPANT_IID \
{ 0x0e2a5a8d, 0x28fd, 0x4a5c, \
  {0x8b, 0xf1, 0x5b, 0x00, 0x67, 0xff, 0x32, 0x86} }

/**
 * DOM GC Participants are objects that expose information about
 * reachability in the native object graphs to help prevent script ->
 * native -> script cyclical reference from causing leaks due to the
 * creation of garbage collection roots and native/script boundaries.
 *
 * Some implementations of nsIDOMGCParticipant may be responsible for
 * enforcing the requirement that callers of
 * |nsDOMClassInfo::PreserveWrapper| must call
 * |nsDOMClassInfo::ReleaseWrapper| before the nsIDOMGCParticipant
 * argument to the former is destroyed.
 */
class nsIDOMGCParticipant : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOMGCPARTICIPANT_IID)

  /**
   * Get a reference node for what is known to be a strongly connected
   * component of nsIDOMGCParticipants.  For example, DOM trees are
   * strongly connected, so can return the root node to greatly reduce
   * the number of nodes on which we need to run graph algorithms.
   *
   * Note that it's acceptable for nodes in a single strongly connected
   * component to return different values for GetSCCIndex, as long as
   * those two values claim that they're reachable from each other in
   * AppendReachableList.
   */
  virtual nsIDOMGCParticipant* GetSCCIndex() = 0;

  /**
   * Append the list of nsIDOMGCPartipants reachable from this one via
   * C++ getters exposed to script that return a different result from
   * |GetSCCIndex|.  The caller is responsible for taking the transitive
   * closure of |AppendReachableList|.
   *
   * This will only be called on objects that are returned by GetSCCIndex.
   *
   * null pointers may be appended; they will be ignored by the caller.
   */
  virtual void AppendReachableList(nsCOMArray<nsIDOMGCParticipant>& aArray) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMGCParticipant, NS_IDOMGCPARTICIPANT_IID)

#endif // !defined(nsIDOMGCParticipant_h_)
