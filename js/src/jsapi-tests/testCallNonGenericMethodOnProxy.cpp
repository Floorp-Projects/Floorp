/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CallAndConstruct.h"
#include "js/GlobalObject.h"
#include "js/Object.h"  // JS::GetClass, JS::GetReservedSlot
#include "jsapi-tests/tests.h"

using namespace JS;

static const JSClass CustomClass = {"CustomClass",
                                    JSCLASS_HAS_RESERVED_SLOTS(1)};

static const uint32_t CUSTOM_SLOT = 0;

static bool IsCustomClass(JS::Handle<JS::Value> v) {
  return v.isObject() && JS::GetClass(&v.toObject()) == &CustomClass;
}

static bool CustomMethodImpl(JSContext* cx, const CallArgs& args) {
  MOZ_RELEASE_ASSERT(IsCustomClass(args.thisv()));
  args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), CUSTOM_SLOT));
  return true;
}

static bool CustomMethod(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, IsCustomClass, CustomMethodImpl, args);
}

BEGIN_TEST(test_CallNonGenericMethodOnProxy) {
  // Create the first global object and compartment
  JS::RealmOptions options;
  JS::RootedObject globalA(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, options));
  CHECK(globalA);

  JS::RootedObject customA(cx, JS_NewObject(cx, &CustomClass));
  CHECK(customA);
  JS_SetReservedSlot(customA, CUSTOM_SLOT, Int32Value(17));

  JS::RootedFunction customMethodA(
      cx, JS_NewFunction(cx, CustomMethod, 0, 0, "customMethodA"));
  CHECK(customMethodA);

  JS::RootedValue rval(cx);
  CHECK(JS_CallFunction(cx, customA, customMethodA,
                        JS::HandleValueArray::empty(), &rval));
  CHECK_SAME(rval, Int32Value(17));

  // Now create the second global object and compartment...
  {
    JS::RealmOptions options;
    JS::RootedObject globalB(
        cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                               JS::FireOnNewGlobalHook, options));
    CHECK(globalB);

    // ...and enter it.
    JSAutoRealm enter(cx, globalB);
    JS::RootedObject customB(cx, JS_NewObject(cx, &CustomClass));
    CHECK(customB);
    JS_SetReservedSlot(customB, CUSTOM_SLOT, Int32Value(42));

    JS::RootedFunction customMethodB(
        cx, JS_NewFunction(cx, CustomMethod, 0, 0, "customMethodB"));
    CHECK(customMethodB);

    JS::RootedValue rval(cx);
    CHECK(JS_CallFunction(cx, customB, customMethodB,
                          JS::HandleValueArray::empty(), &rval));
    CHECK_SAME(rval, Int32Value(42));

    JS::RootedObject wrappedCustomA(cx, customA);
    CHECK(JS_WrapObject(cx, &wrappedCustomA));

    JS::RootedValue rval2(cx);
    CHECK(JS_CallFunction(cx, wrappedCustomA, customMethodB,
                          JS::HandleValueArray::empty(), &rval2));
    CHECK_SAME(rval, Int32Value(42));
  }

  return true;
}
END_TEST(test_CallNonGenericMethodOnProxy)
