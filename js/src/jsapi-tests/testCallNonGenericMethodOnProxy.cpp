/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using namespace JS;

static const JSClass CustomClass = {
  "CustomClass",
  JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub,
  JS_DeletePropertyStub,
  JS_PropertyStub,
  JS_StrictPropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub
};

static const uint32_t CUSTOM_SLOT = 0;

static bool
IsCustomClass(JS::Handle<JS::Value> v)
{
  return v.isObject() && JS_GetClass(&v.toObject()) == &CustomClass;
}

static bool
CustomMethodImpl(JSContext *cx, CallArgs args)
{
  JS_ASSERT(IsCustomClass(args.thisv()));
  args.rval().set(JS_GetReservedSlot(&args.thisv().toObject(), CUSTOM_SLOT));
  return true;
}

static bool
CustomMethod(JSContext *cx, unsigned argc, Value *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, IsCustomClass, CustomMethodImpl, args);
}

BEGIN_TEST(test_CallNonGenericMethodOnProxy)
{
  // Create the first global object and compartment
  JS::RootedObject globalA(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr, JS::FireOnNewGlobalHook));
  CHECK(globalA);

  JS::RootedObject customA(cx, JS_NewObject(cx, &CustomClass, JS::NullPtr(), JS::NullPtr()));
  CHECK(customA);
  JS_SetReservedSlot(customA, CUSTOM_SLOT, Int32Value(17));

  JSFunction *customMethodA = JS_NewFunction(cx, CustomMethod, 0, 0, customA, "customMethodA");
  CHECK(customMethodA);

  JS::RootedValue rval(cx);
  CHECK(JS_CallFunction(cx, customA, customMethodA, JS::EmptyValueArray, rval.address()));
  CHECK_SAME(rval, Int32Value(17));

  // Now create the second global object and compartment...
  {
    JS::RootedObject globalB(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr, JS::FireOnNewGlobalHook));
    CHECK(globalB);

    // ...and enter it.
    JSAutoCompartment enter(cx, globalB);
    JS::RootedObject customB(cx, JS_NewObject(cx, &CustomClass, JS::NullPtr(), JS::NullPtr()));
    CHECK(customB);
    JS_SetReservedSlot(customB, CUSTOM_SLOT, Int32Value(42));

    JS::RootedFunction customMethodB(cx, JS_NewFunction(cx, CustomMethod, 0, 0, customB, "customMethodB"));
    CHECK(customMethodB);

    JS::RootedValue rval(cx);
    CHECK(JS_CallFunction(cx, customB, customMethodB, JS::EmptyValueArray, rval.address()));
    CHECK_SAME(rval, Int32Value(42));

    JS::RootedObject wrappedCustomA(cx, customA);
    CHECK(JS_WrapObject(cx, &wrappedCustomA));

    JS::RootedValue rval2(cx);
    CHECK(JS_CallFunction(cx, wrappedCustomA, customMethodB, JS::EmptyValueArray, rval2.address()));
    CHECK_SAME(rval, Int32Value(42));
  }

  return true;
}
END_TEST(test_CallNonGenericMethodOnProxy)
