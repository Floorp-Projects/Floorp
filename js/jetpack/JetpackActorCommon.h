/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Newman <mozilla@benjamn.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_jetpack_JetpackActorCommon_h
#define mozilla_jetpack_JetpackActorCommon_h

#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsAutoJSValHolder.h"

struct JSContext;

namespace mozilla {
namespace jetpack {

class KeyValue;
class PrimVariant;
class CompVariant;
class Variant;

class JetpackActorCommon
{
public:

  bool
  RecvMessage(JSContext* cx,
              const nsString& messageName,
              const InfallibleTArray<Variant>& data,
              InfallibleTArray<Variant>* results);

  nsresult
  RegisterReceiver(JSContext* cx,
                   const nsString& messageName,
                   jsval receiver);

  void
  UnregisterReceiver(const nsString& messageName,
                     jsval receiver);

  void
  UnregisterReceivers(const nsString& messageName) {
    mReceivers.Remove(messageName);
  }

  void ClearReceivers() {
    mReceivers.Clear();
  }

  class OpaqueSeenType;
  static bool jsval_to_Variant(JSContext* cx, jsval from, Variant* to,
                               OpaqueSeenType* seen = NULL);
  static bool jsval_from_Variant(JSContext* cx, const Variant& from, jsval* to,
                                 OpaqueSeenType* seen = NULL);

protected:

  JetpackActorCommon() {
    mReceivers.Init();
    NS_ASSERTION(mReceivers.IsInitialized(),
                 "Failed to initialize message receiver hash set");
  }

private:

  static bool jsval_to_PrimVariant(JSContext* cx, JSType type, jsval from,
                                   PrimVariant* to);
  static bool jsval_to_CompVariant(JSContext* cx, JSType type, jsval from,
                                   CompVariant* to, OpaqueSeenType* seen);

  static bool jsval_from_PrimVariant(JSContext* cx, const PrimVariant& from,
                                     jsval* to);
  static bool jsval_from_CompVariant(JSContext* cx, const CompVariant& from,
                                     jsval* to, OpaqueSeenType* seen);

  // Don't want to be memcpy'ing nsAutoJSValHolders around, so we need a
  // linked list of receivers.
  class RecList
  {
    JSContext* mCx;
    class RecNode
    {
      nsAutoJSValHolder mHolder;
    public:
      RecNode* down;
      RecNode(JSContext* cx, jsval v) : down(NULL) {
        mHolder.Hold(cx);
        mHolder = v;
      }
      jsval value() { return mHolder; }
    }* mHead;
  public:
    RecList(JSContext* cx) : mCx(cx), mHead(NULL) {}
   ~RecList();
    void add(jsval v);
    void remove(jsval v);
    void copyTo(nsTArray<jsval>& dst) const;
  };

  nsClassHashtable<nsStringHashKey, RecList> mReceivers;

};

} // namespace jetpack
} // namespace mozilla

#endif
