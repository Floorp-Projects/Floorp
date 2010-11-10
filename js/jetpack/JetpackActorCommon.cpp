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

#include "base/basictypes.h"
#include "jscntxt.h"

#include "jsapi.h"
#include "jstl.h"
#include "jshashtable.h"

#include "mozilla/jetpack/JetpackActorCommon.h"
#include "mozilla/jetpack/PJetpack.h"
#include "mozilla/jetpack/PHandleParent.h"
#include "mozilla/jetpack/PHandleChild.h"
#include "mozilla/jetpack/Handle.h"

using mozilla::jetpack::JetpackActorCommon;
using mozilla::jetpack::PHandleParent;
using mozilla::jetpack::HandleParent;
using mozilla::jetpack::PHandleChild;
using mozilla::jetpack::HandleChild;
using mozilla::jetpack::KeyValue;
using mozilla::jetpack::PrimVariant;
using mozilla::jetpack::CompVariant;
using mozilla::jetpack::Variant;

class JetpackActorCommon::OpaqueSeenType
{
public:
  typedef JSObject* KeyType;
  typedef size_t IdType;
  typedef js::HashMap<
    KeyType, IdType,
    js::DefaultHasher<KeyType>,
    js::SystemAllocPolicy
  > MapType;

  OpaqueSeenType() {
    (void) map.init(1);
  }

  bool ok() { return map.initialized(); }

  // Reserving 0 as an invalid ID means starting the valid IDs at 1.  We
  // could have reserved a dummy first element of rmap, but that would
  // have wasted space.
  static const IdType kInvalidId = 0;

  bool add(KeyType obj, IdType* id) {
    MapType::AddPtr ap = map.lookupForAdd(obj);
    if (!ap) {
      if (!rmap.AppendElement(obj) ||
          !map.add(ap, obj, *id = rmap.Length()))
        *id = kInvalidId;
      return true;
    }
    *id = ap->value;
    return false;
  }

  KeyType reverseLookup(IdType id) {
    return rmap.SafeElementAt(id - 1, NULL);
  }

private:
  MapType map;
  nsAutoTArray<KeyType, 4> rmap;

};

bool
JetpackActorCommon::jsval_to_PrimVariant(JSContext* cx, JSType type, jsval from,
                                         PrimVariant* to)
{
  // A false return from this function just means the value couldn't be
  // converted to a PrimVariant (i.e., it wasn't a primitive value).

  switch (type) {
  case JSTYPE_VOID:
    *to = void_t();
    return true;

  case JSTYPE_NULL:
    *to = null_t();
    return true;

  case JSTYPE_FUNCTION:
    return false;

  case JSTYPE_OBJECT: {
    HandleParent* hp = HandleParent::FromJSVal(cx, from);
    HandleChild* hc = HandleChild::FromJSVal(cx, from);
    NS_ASSERTION(!hc || !hp, "Can't be both a parent and a child");
    if (hp) {
      *to = hp;
      return true;
    }
    if (hc) {
      *to = hc;
      return true;
    }
    return false;
  }

  case JSTYPE_STRING:
    *to = nsDependentString((PRUnichar*)JS_GetStringChars(JSVAL_TO_STRING(from)),
                            JS_GetStringLength(JSVAL_TO_STRING(from)));
    return true;

  case JSTYPE_NUMBER:
    if (JSVAL_IS_INT(from))
      *to = JSVAL_TO_INT(from);
    else if (JSVAL_IS_DOUBLE(from))
      *to = JSVAL_TO_DOUBLE(from);
    else
      return false;
    return true;

  case JSTYPE_BOOLEAN:
    *to = !!JSVAL_TO_BOOLEAN(from);
    return true;

  case JSTYPE_XML:
    return false;

  default:
    return false;
  }
}

bool
JetpackActorCommon::jsval_to_CompVariant(JSContext* cx, JSType type, jsval from,
                                         CompVariant* to, OpaqueSeenType* seen)
{
  if (type != JSTYPE_OBJECT)
    return false;

  js::LazilyConstructed<OpaqueSeenType> lost;
  if (!seen) {
    lost.construct();
    seen = lost.addr();
    if (!seen->ok())
      return false;
  }

  OpaqueSeenType::KeyType obj = JSVAL_TO_OBJECT(from);
  OpaqueSeenType::IdType id;
  if (!seen->add(obj, &id)) {
    if (OpaqueSeenType::kInvalidId == id)
      return false;
    *to = CompVariant(id);
    return true;
  }

  if (JS_IsArrayObject(cx, obj)) {
    nsTArray<Variant> elems;
    jsuint len;
    if (!JS_GetArrayLength(cx, obj, &len) ||
        !elems.SetCapacity(len))
      return false;
    for (jsuint i = 0; i < len; ++i) {
      jsval val;
      Variant* vp = elems.AppendElement();
      if (!JS_GetElement(cx, obj, i, &val) ||
          !jsval_to_Variant(cx, val, vp, seen))
        *vp = void_t();
    }
    InfallibleTArray<Variant> outElems;
    outElems.SwapElements(elems);
    *to = outElems;
    return true;
  }

  js::AutoIdArray ida(cx, JS_Enumerate(cx, obj));
  if (!ida)
    return false;

  nsTArray<KeyValue> kvs;
  for (size_t i = 0; i < ida.length(); ++i) {
    jsval val; // reused for both key and value
    if (!JS_IdToValue(cx, ida[i], &val))
      return false;
    JSString* idStr = JS_ValueToString(cx, val);
    if (!idStr)
      return false;
    if (!JS_GetPropertyById(cx, obj, ida[i], &val))
      return false;
    KeyValue kv;
    // Silently drop properties that can't be converted.
    if (jsval_to_Variant(cx, val, &kv.value(), seen)) {
      kv.key() = nsDependentString((PRUnichar*)JS_GetStringChars(idStr),
                                   JS_GetStringLength(idStr));
      // If AppendElement fails, we lose this property, no big deal.
      kvs.AppendElement(kv);
    }
  }
  InfallibleTArray<KeyValue> outKvs;
  outKvs.SwapElements(kvs);
  *to = outKvs;

  return true;
}

bool
JetpackActorCommon::jsval_to_Variant(JSContext* cx, jsval from, Variant* to,
                                     OpaqueSeenType* seen)
{
  JSType type = JS_TypeOfValue(cx, from);
  if (JSVAL_IS_NULL(from))
    type = JSTYPE_NULL;

  PrimVariant pv;
  if (jsval_to_PrimVariant(cx, type, from, &pv)) {
    *to = pv;
    return true;
  }

  CompVariant cv;
  if (jsval_to_CompVariant(cx, type, from, &cv, seen)) {
    *to = cv;
    return true;
  }

  return false;
}

bool
JetpackActorCommon::jsval_from_PrimVariant(JSContext* cx,
                                           const PrimVariant& from,
                                           jsval* to)
{
  switch (from.type()) {
  case PrimVariant::Tvoid_t:
    *to = JSVAL_VOID;
    return true;

  case PrimVariant::Tnull_t:
    *to = JSVAL_NULL;
    return true;

  case PrimVariant::Tbool:
    *to = from.get_bool() ? JSVAL_TRUE : JSVAL_FALSE;
    return true;

  case PrimVariant::Tint:
    *to = INT_TO_JSVAL(from.get_int());
    return true;

  case PrimVariant::Tdouble:
    return !!JS_NewNumberValue(cx, from.get_double(), to);

  case PrimVariant::TnsString: {
    const nsString& str = from.get_nsString();
    // TODO Use some sort of sharedstring/stringbuffer abstraction to
    // exploit sharing opportunities more generally.
    if (!str.Length()) {
      *to = JS_GetEmptyStringValue(cx);
      return true;
    }
    JSString* s =
      JS_NewUCStringCopyN(cx, str.get(), str.Length());
    if (!s)
      return false;
    *to = STRING_TO_JSVAL(s);
    return true;
  }

  case PrimVariant::TPHandleParent: {
    JSObject* hobj =
      static_cast<HandleParent*>(from.get_PHandleParent())->ToJSObject(cx);
    if (!hobj)
      return false;
    *to = OBJECT_TO_JSVAL(hobj);
    return true;
  }    

  case PrimVariant::TPHandleChild: {
    JSObject* hobj =
      static_cast<HandleChild*>(from.get_PHandleChild())->ToJSObject(cx);
    if (!hobj)
      return false;
    *to = OBJECT_TO_JSVAL(hobj);
    return true;
  }

  default:
    return false;
  }
}

bool
JetpackActorCommon::jsval_from_CompVariant(JSContext* cx,
                                           const CompVariant& from,
                                           jsval* to,
                                           OpaqueSeenType* seen)
{
  js::LazilyConstructed<OpaqueSeenType> lost;
  if (!seen) {
    lost.construct();
    seen = lost.addr();
    if (!seen->ok())
      return false;
  }

  JSObject* obj = NULL;

  switch (from.type()) {
  case CompVariant::TArrayOfKeyValue: {
    if (!(obj = JS_NewObject(cx, NULL, NULL, NULL)))
      return false;
    js::AutoObjectRooter root(cx, obj);

    OpaqueSeenType::IdType ignored;
    if (!seen->add(obj, &ignored))
      return false;

    const nsTArray<KeyValue>& kvs = from.get_ArrayOfKeyValue();
    for (PRUint32 i = 0; i < kvs.Length(); ++i) {
      const KeyValue& kv = kvs.ElementAt(i);
      js::AutoValueRooter toSet(cx);
      if (!jsval_from_Variant(cx, kv.value(), toSet.jsval_addr(), seen) ||
          !JS_SetUCProperty(cx, obj,
                            kv.key().get(),
                            kv.key().Length(),
                            toSet.jsval_addr()))
        return false;
    }

    break;
  }

  case CompVariant::TArrayOfVariant: {
    const nsTArray<Variant>& vs = from.get_ArrayOfVariant();
    nsAutoTArray<jsval, 8> jsvals;
    jsval* elems = jsvals.AppendElements(vs.Length());
    if (!elems)
      return false;
    for (PRUint32 i = 0; i < vs.Length(); ++i)
      elems[i] = JSVAL_VOID;
    js::AutoArrayRooter root(cx, vs.Length(), elems);

    OpaqueSeenType::IdType ignored;
    if (!seen->add(obj, &ignored))
      return false;

    for (PRUint32 i = 0; i < vs.Length(); ++i)
      if (!jsval_from_Variant(cx, vs.ElementAt(i), elems + i, seen))
        return false;

    if (!(obj = JS_NewArrayObject(cx, vs.Length(), elems)))
      return false;

    break;
  }

  case CompVariant::Tsize_t:
    if (!(obj = seen->reverseLookup(from.get_size_t())))
      return false;
    break;

  default:
    return false;
  }

  *to = OBJECT_TO_JSVAL(obj);
  return true;
}

bool
JetpackActorCommon::jsval_from_Variant(JSContext* cx, const Variant& from,
                                       jsval* to, OpaqueSeenType* seen)
{
  switch (from.type()) {
  case Variant::TPrimVariant:
    return jsval_from_PrimVariant(cx, from, to);
  case Variant::TCompVariant:
    return jsval_from_CompVariant(cx, from, to, seen);
  default:
    return false;
  }
}

bool
JetpackActorCommon::RecvMessage(JSContext* cx,
                                const nsString& messageName,
                                const InfallibleTArray<Variant>& data,
                                InfallibleTArray<Variant>* results)
{
  if (results)
    results->Clear();

  RecList* list;
  if (!mReceivers.Get(messageName, &list))
    return true;
  nsAutoTArray<jsval, 4> snapshot;
  list->copyTo(snapshot);
  if (!snapshot.Length())
    return true;
  
  nsAutoTArray<jsval, 4> args;
  PRUint32 argc = data.Length() + 1;
  jsval* argv = args.AppendElements(argc);
  if (!argv)
    return false;
  for (PRUint32 i = 0; i < argc; ++i)
    argv[i] = JSVAL_VOID;
  js::AutoArrayRooter argvRooter(cx, argc, argv);

  JSString* msgNameStr =
    JS_NewUCStringCopyN(cx,
                        messageName.get(),
                        messageName.Length());
  if (!msgNameStr)
    return false;
  argv[0] = STRING_TO_JSVAL(msgNameStr);

  for (PRUint32 i = 0; i < data.Length(); ++i)
    if (!jsval_from_Variant(cx, data.ElementAt(i), argv + i + 1))
      return false;

  JSObject* implGlobal = JS_GetGlobalObject(cx);
  js::AutoValueRooter rval(cx);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, implGlobal))
    return false;

  for (PRUint32 i = 0; i < snapshot.Length(); ++i) {
    Variant* vp = results ? results->AppendElement() : NULL;
    rval.set(JSVAL_VOID);
    if (!JS_CallFunctionValue(cx, implGlobal, snapshot[i], argc, argv,
                              rval.jsval_addr())) {
      (void) JS_ReportPendingException(cx);
      if (vp)
        *vp = void_t();
    } else if (vp && !jsval_to_Variant(cx, rval.jsval_value(), vp))
      *vp = void_t();
  }

  return true;
}

JetpackActorCommon::RecList::~RecList()
{
  while (mHead) {
    RecNode* old = mHead;
    mHead = mHead->down;
    delete old;
  }
}

void
JetpackActorCommon::RecList::add(jsval v)
{
  RecNode* node = mHead, *tail = NULL;
  while (node) {
    if (node->value() == v)
      return;
    node = (tail = node)->down;
  }
  node = new RecNode(mCx, v);
  if (tail)
    tail->down = node;
  else
    mHead = node;
}

void
JetpackActorCommon::RecList::remove(jsval v)
{
  while (mHead && mHead->value() == v) {
    RecNode* old = mHead;
    mHead = mHead->down;
    delete old;
  }
  if (!mHead)
    return;
  RecNode* prev = mHead, *node = prev->down;
  while (node) {
    if (node->value() == v) {
      prev->down = node->down;
      delete node;
    }
    node = (prev = node)->down;
  }
}

void
JetpackActorCommon::RecList::copyTo(nsTArray<jsval>& dst) const
{
  dst.Clear();
  for (RecNode* node = mHead; node; node = node->down)
    dst.AppendElement(node->value());
}

nsresult
JetpackActorCommon::RegisterReceiver(JSContext* cx,
                                     const nsString& messageName,
                                     jsval receiver)
{
  if (!JSVAL_IS_OBJECT(receiver) ||
      !JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(receiver)))
    return NS_ERROR_INVALID_ARG;

  RecList* list;
  if (!mReceivers.Get(messageName, &list)) {
    list = new RecList(cx);
    if (!list || !mReceivers.Put(messageName, list)) {
      delete list;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  list->add(receiver);

  return NS_OK;
}

void
JetpackActorCommon::UnregisterReceiver(const nsString& messageName,
                                       jsval receiver)
{
  RecList* list;
  if (!mReceivers.Get(messageName, &list))
    return;
  list->remove(receiver);
}
