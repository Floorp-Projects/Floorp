/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

using namespace JS;

static JSClass CustomClass = {
  "CustomClass",
  JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_StrictPropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub
};

static const uint32_t CUSTOM_SLOT = 0;

static bool
IsCustomClass(const Value &v)
{
  return v.isObject() && JS_GetClass(&v.toObject()) == &CustomClass;
}

static bool
CustomMethodImpl(JSContext *cx, CallArgs args)
{
  JS_ASSERT(IsCustomClass(args.thisv()));
  args.rval() = JS_GetReservedSlot(&args.thisv().toObject(), CUSTOM_SLOT);
  return true;
}

static JSBool
CustomMethod(JSContext *cx, unsigned argc, Value *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, IsCustomClass, CustomMethodImpl, args);
}

BEGIN_TEST(test_CallNonGenericMethodOnProxy)
{
  // Create the first global object and compartment
  JSObject *globalA = JS_NewGlobalObject(cx, getGlobalClass(), NULL);
  CHECK(globalA);

  JSObject *customA = JS_NewObject(cx, &CustomClass, NULL, NULL);
  CHECK(customA);
  JS_SetReservedSlot(customA, CUSTOM_SLOT, Int32Value(17));

  JSFunction *customMethodA = JS_NewFunction(cx, CustomMethod, 0, 0, customA, "customMethodA");
  CHECK(customMethodA);

  jsval rval;
  CHECK(JS_CallFunction(cx, customA, customMethodA, 0, NULL, &rval));
  CHECK_SAME(rval, Int32Value(17));

  // Now create the second global object and compartment...
  {
    JSObject *globalB = JS_NewGlobalObject(cx, getGlobalClass(), NULL);
    CHECK(globalB);

    // ...and enter it.
    JSAutoEnterCompartment enter;
    CHECK(enter.enter(cx, globalB));

    JSObject *customB = JS_NewObject(cx, &CustomClass, NULL, NULL);
    CHECK(customB);
    JS_SetReservedSlot(customB, CUSTOM_SLOT, Int32Value(42));

    JSFunction *customMethodB = JS_NewFunction(cx, CustomMethod, 0, 0, customB, "customMethodB");
    CHECK(customMethodB);

    jsval rval;
    CHECK(JS_CallFunction(cx, customB, customMethodB, 0, NULL, &rval));
    CHECK_SAME(rval, Int32Value(42));

    JSObject *wrappedCustomA = customA;
    CHECK(JS_WrapObject(cx, &wrappedCustomA));

    jsval rval2;
    CHECK(JS_CallFunction(cx, wrappedCustomA, customMethodB, 0, NULL, &rval2));
    CHECK_SAME(rval, Int32Value(42));
  }

  return true;
}
END_TEST(test_CallNonGenericMethodOnProxy)
