/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAllCollection.h"

#include "jsapi.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsHTMLDocument.h"
#include "nsJSUtils.h"
#include "nsWrapperCacheInlines.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsHTMLDocumentSH
{
public:
  static bool DocumentAllGetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                                     JS::MutableHandle<JS::Value> vp);
  static bool DocumentAllNewResolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                                    unsigned flags, JS::MutableHandle<JSObject*> objp);
  static void ReleaseDocument(JSFreeOp *fop, JSObject *obj);
  static bool CallToGetPropMapper(JSContext *cx, unsigned argc, JS::Value *vp);
};

const JSClass sHTMLDocumentAllClass = {
  "HTML document.all class",
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_NEW_RESOLVE |
  JSCLASS_EMULATES_UNDEFINED,
  JS_PropertyStub,                                         /* addProperty */
  JS_DeletePropertyStub,                                   /* delProperty */
  nsHTMLDocumentSH::DocumentAllGetProperty,                /* getProperty */
  JS_StrictPropertyStub,                                   /* setProperty */
  JS_EnumerateStub,                                        /* enumerate */
  (JSResolveOp)nsHTMLDocumentSH::DocumentAllNewResolve,    /* resolve */
  JS_ConvertStub,                                          /* convert */
  nsHTMLDocumentSH::ReleaseDocument                        /* finalize */
};

namespace mozilla {
namespace dom {

HTMLAllCollection::HTMLAllCollection(nsHTMLDocument* aDocument)
  : mDocument(aDocument)
{
  MOZ_ASSERT(mDocument);
  mozilla::HoldJSObjects(this);
}

HTMLAllCollection::~HTMLAllCollection()
{
  mObject = nullptr;
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(HTMLAllCollection, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(HTMLAllCollection, Release)

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLAllCollection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HTMLAllCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNamedMap)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLAllCollection)
  tmp->mObject = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCollection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNamedMap)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HTMLAllCollection)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

uint32_t
HTMLAllCollection::Length()
{
  return Collection()->Length(true);
}

nsIContent*
HTMLAllCollection::Item(uint32_t aIndex)
{
  return Collection()->Item(aIndex);
}

JSObject*
HTMLAllCollection::GetObject(JSContext* aCx, ErrorResult& aRv)
{
  MOZ_ASSERT(aCx);

  if (!mObject) {
    JS::Rooted<JSObject*> wrapper(aCx, mDocument->GetWrapper());
    MOZ_ASSERT(wrapper);

    JSAutoCompartment ac(aCx, wrapper);
    JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForObject(aCx, wrapper));
    mObject = JS_NewObject(aCx, &sHTMLDocumentAllClass, JS::NullPtr(), global);
    if (!mObject) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    // Make the JSObject hold a reference to the document.
    JS_SetPrivate(mObject, ToSupports(mDocument));
    NS_ADDREF(mDocument);
  }

  JS::ExposeObjectToActiveJS(mObject);
  return mObject;
}

nsContentList*
HTMLAllCollection::Collection()
{
  if (!mCollection) {
    nsIDocument* document = mDocument;
    mCollection = document->GetElementsByTagName(NS_LITERAL_STRING("*"));
    MOZ_ASSERT(mCollection);
  }
  return mCollection;
}

static bool
DocAllResultMatch(nsIContent* aContent, int32_t aNamespaceID, nsIAtom* aAtom,
                  void* aData)
{
  if (aContent->GetID() == aAtom) {
    return true;
  }

  nsGenericHTMLElement* elm = nsGenericHTMLElement::FromContent(aContent);
  if (!elm) {
    return false;
  }

  nsIAtom* tag = elm->Tag();
  if (tag != nsGkAtoms::a &&
      tag != nsGkAtoms::applet &&
      tag != nsGkAtoms::button &&
      tag != nsGkAtoms::embed &&
      tag != nsGkAtoms::form &&
      tag != nsGkAtoms::iframe &&
      tag != nsGkAtoms::img &&
      tag != nsGkAtoms::input &&
      tag != nsGkAtoms::map &&
      tag != nsGkAtoms::meta &&
      tag != nsGkAtoms::object &&
      tag != nsGkAtoms::select &&
      tag != nsGkAtoms::textarea) {
    return false;
  }

  const nsAttrValue* val = elm->GetParsedAttr(nsGkAtoms::name);
  return val && val->Type() == nsAttrValue::eAtom &&
         val->GetAtomValue() == aAtom;
}

nsContentList*
HTMLAllCollection::GetDocumentAllList(const nsAString& aID)
{
  if (nsContentList* docAllList = mNamedMap.GetWeak(aID)) {
    return docAllList;
  }

  Element* root = mDocument->GetRootElement();
  if (!root) {
    return nullptr;
  }

  nsCOMPtr<nsIAtom> id = do_GetAtom(aID);
  nsRefPtr<nsContentList> docAllList =
    new nsContentList(root, DocAllResultMatch, nullptr, nullptr, true, id);
  mNamedMap.Put(aID, docAllList);
  return docAllList;
}

nsISupports*
HTMLAllCollection::GetNamedItem(const nsAString& aID,
                                nsWrapperCache** aCache)
{
  nsContentList* docAllList = GetDocumentAllList(aID);
  if (!docAllList) {
    return nullptr;
  }

  // Check if there are more than 1 entries. Do this by getting the second one
  // rather than the length since getting the length always requires walking
  // the entire document.

  nsIContent* cont = docAllList->Item(1, true);
  if (cont) {
    *aCache = docAllList;
    return static_cast<nsINodeList*>(docAllList);
  }

  // There's only 0 or 1 items. Return the first one or null.
  *aCache = cont = docAllList->Item(0, true);
  return cont;
}

} // namespace dom
} // namespace mozilla

static nsHTMLDocument*
GetDocument(JSObject *obj)
{
  MOZ_ASSERT(js::GetObjectJSClass(obj) == &sHTMLDocumentAllClass);
  return static_cast<nsHTMLDocument*>(
    static_cast<nsINode*>(JS_GetPrivate(obj)));
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, const nsIID* aIID, JS::MutableHandle<JS::Value> vp,
           bool aAllowWrapping)
{
  if (!native) {
    vp.setNull();

    return NS_OK;
  }

  JSObject *wrapper = xpc_FastGetCachedWrapper(cache, scope, vp);
  if (wrapper) {
    return NS_OK;
  }

  return nsDOMClassInfo::XPConnect()->WrapNativeToJSVal(cx, scope, native,
                                                        cache, aIID,
                                                        aAllowWrapping, vp);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, bool aAllowWrapping,
           JS::MutableHandle<JS::Value> vp)
{
  return WrapNative(cx, scope, native, cache, nullptr, vp, aAllowWrapping);
}

bool
nsHTMLDocumentSH::DocumentAllGetProperty(JSContext *cx, JS::Handle<JSObject*> obj_,
                                         JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp)
{
  JS::Rooted<JSObject*> obj(cx, obj_);

  // document.all.item and .namedItem get their value in the
  // newResolve hook, so nothing to do for those properties here. And
  // we need to return early to prevent <div id="item"> from shadowing
  // document.all.item(), etc.
  if (nsDOMClassInfo::sItem_id == id || nsDOMClassInfo::sNamedItem_id == id) {
    return true;
  }

  JS::Rooted<JSObject*> proto(cx);
  while (js::GetObjectJSClass(obj) != &sHTMLDocumentAllClass) {
    if (!js::GetObjectProto(cx, obj, &proto)) {
      return false;
    }

    if (!proto) {
      NS_ERROR("The JS engine lies!");
      return true;
    }

    obj = proto;
  }

  HTMLAllCollection* allCollection = GetDocument(obj)->All();
  nsISupports *result;
  nsWrapperCache *cache;

  if (JSID_IS_STRING(id)) {
    if (nsDOMClassInfo::sLength_id == id) {
      // Make sure <div id="length"> doesn't shadow document.all.length.
      vp.setNumber(allCollection->Length());
      return true;
    }

    // For all other strings, look for an element by id or name.
    nsDependentJSString str(id);
    result = allCollection->GetNamedItem(str, &cache);
  } else if (JSID_IS_INT(id) && JSID_TO_INT(id) >= 0) {
    // Map document.all[n] (where n is a number) to the n:th item in
    // the document.all node list.

    nsIContent* node = allCollection->Item(SafeCast<uint32_t>(JSID_TO_INT(id)));

    result = node;
    cache = node;
  } else {
    result = nullptr;
  }

  if (result) {
    nsresult rv = WrapNative(cx, JS::CurrentGlobalOrNull(cx), result, cache,
                             true, vp);
    if (NS_FAILED(rv)) {
      xpc::Throw(cx, rv);

      return false;
    }
  } else {
    vp.setUndefined();
  }

  return true;
}

bool
nsHTMLDocumentSH::DocumentAllNewResolve(JSContext *cx, JS::Handle<JSObject*> obj,
                                        JS::Handle<jsid> id, unsigned flags,
                                        JS::MutableHandle<JSObject*> objp)
{
  JS::Rooted<JS::Value> v(cx);

  if (nsDOMClassInfo::sItem_id == id || nsDOMClassInfo::sNamedItem_id == id) {
    // Define the item() or namedItem() method.

    JSFunction *fnc = ::JS_DefineFunctionById(cx, obj, id, CallToGetPropMapper,
                                              0, JSPROP_ENUMERATE);
    objp.set(obj);

    return fnc != nullptr;
  }

  if (nsDOMClassInfo::sLength_id == id) {
    // document.all.length. Any jsval other than undefined would do
    // here, all we need is to get into the code below that defines
    // this propery on obj, the rest happens in
    // DocumentAllGetProperty().

    v = JSVAL_ONE;
  } else {
    if (!DocumentAllGetProperty(cx, obj, id, &v)) {
      return false;
    }
  }

  bool ok = true;

  if (v.get() != JSVAL_VOID) {
    ok = ::JS_DefinePropertyById(cx, obj, id, v, nullptr, nullptr, 0);
    objp.set(obj);
  }

  return ok;
}

void
nsHTMLDocumentSH::ReleaseDocument(JSFreeOp *fop, JSObject *obj)
{
  nsIHTMLDocument* doc = GetDocument(obj);
  if (doc) {
    nsContentUtils::DeferredFinalize(doc);
  }
}

bool
nsHTMLDocumentSH::CallToGetPropMapper(JSContext *cx, unsigned argc, jsval *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    // XXX: Should throw NS_ERROR_XPC_NOT_ENOUGH_ARGS for argc < 1,
    // and create a new NS_ERROR_XPC_TOO_MANY_ARGS for argc > 1? IE
    // accepts nothing other than one arg.
    xpc::Throw(cx, NS_ERROR_INVALID_ARG);
    return false;
  }

  // Convert all types to string.
  JS::Rooted<JSString*> str(cx, JS::ToString(cx, args[0]));
  if (!str) {
    return false;
  }

  JS::Rooted<JSObject*> self(cx, JS_THIS_OBJECT(cx, vp));
  if (!self) {
    return false;
  }

  size_t length;
  JS::Anchor<JSString *> anchor(str);
  const jschar *chars = ::JS_GetStringCharsAndLength(cx, str, &length);
  if (!chars) {
    return false;
  }

  return ::JS_GetUCProperty(cx, self, chars, length, args.rval());
}
