/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test JSObject::swap.
 *
 * This test creates objects from a description of their configuration. Objects
 * are given unique property names and values. A list of configurations is
 * created and the result of swapping every combination checked.
 */

#include "js/AllocPolicy.h"
#include "js/Vector.h"
#include "jsapi-tests/tests.h"

#include "vm/JSObject-inl.h"

using namespace js;

struct NativeConfig {
  uint32_t propCount;
  bool inDictionaryMode;
};

struct ProxyConfig {
  bool inlineValues;
};

struct ObjectConfig {
  const JSClass* clasp;
  bool isNative;
  bool isPrototype;
  union {
    NativeConfig native;
    ProxyConfig proxy;
  };
};

using ObjectConfigVector = Vector<ObjectConfig, 0, SystemAllocPolicy>;

static const JSClass TestProxyClasses[] = {
    PROXY_CLASS_DEF("TestProxy", JSCLASS_HAS_RESERVED_SLOTS(1 /* Min */)),
    PROXY_CLASS_DEF("TestProxy", JSCLASS_HAS_RESERVED_SLOTS(2)),
    PROXY_CLASS_DEF("TestProxy", JSCLASS_HAS_RESERVED_SLOTS(7)),
    PROXY_CLASS_DEF("TestProxy", JSCLASS_HAS_RESERVED_SLOTS(8)),
    PROXY_CLASS_DEF("TestProxy", JSCLASS_HAS_RESERVED_SLOTS(14 /* Max */))};

static const JSClass TestDOMClasses[] = {
    {"TestDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(0)},
    {"TestDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(1)},
    {"TestDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2)},
    {"TestDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(7)},
    {"TestDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(8)},
    {"TestDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(20)}};

static const uint32_t TestPropertyCounts[] = {0, 1, 2, 7, 8, 20};

static bool Verbose = false;

class DummyProxyHandler final : public ForwardingProxyHandler {
 public:
  static const DummyProxyHandler singleton;
  static const char family;
  constexpr DummyProxyHandler() : ForwardingProxyHandler(&family) {}
};

const DummyProxyHandler DummyProxyHandler::singleton;
const char DummyProxyHandler::family = 0;

BEGIN_TEST(testObjectSwap) {
  ObjectConfigVector objectConfigs = CreateObjectConfigs();

  for (const ObjectConfig& config1 : objectConfigs) {
    for (const ObjectConfig& config2 : objectConfigs) {
      uint32_t id1;
      RootedObject obj1(cx, CreateObject(config1, &id1));
      CHECK(obj1);

      uint32_t id2;
      RootedObject obj2(cx, CreateObject(config2, &id2));
      CHECK(obj2);

      if (Verbose) {
        fprintf(stderr, "Swap %p and %p\n", obj1.get(), obj2.get());
      }

      {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        JSObject::swap(cx, obj1, obj2, oomUnsafe);
      }

      CHECK(CheckObject(obj1, config2, id2));
      CHECK(CheckObject(obj2, config1, id1));

      if (Verbose) {
        fprintf(stderr, "\n");
      }
    }

    // JSObject::swap can suppress GC so ensure we clean up occasionally.
    JS_GC(cx);
  }

  return true;
}

ObjectConfigVector CreateObjectConfigs() {
  ObjectConfigVector configs;

  ObjectConfig config;
  config.isNative = true;
  config.native = NativeConfig{0, false};

  for (const JSClass& jsClass : TestDOMClasses) {
    config.clasp = &jsClass;

    for (uint32_t propCount : TestPropertyCounts) {
      config.native.propCount = propCount;

      for (bool inDictionaryMode : {false, true}) {
        if (inDictionaryMode && propCount == 0) {
          continue;
        }

        config.native.inDictionaryMode = inDictionaryMode;
        MOZ_RELEASE_ASSERT(configs.append(config));
      }
    }
  }

  config.isNative = false;
  config.proxy = ProxyConfig{false};

  for (const JSClass& jsClass : TestProxyClasses) {
    config.clasp = &jsClass;

    for (bool inlineValues : {false, true}) {
      config.proxy.inlineValues = inlineValues;
      MOZ_RELEASE_ASSERT(configs.append(config));
    }
  }

  return configs;
}

// Counter used to give slots and property names unique values.
uint32_t nextId = 0;

JSObject* CreateObject(const ObjectConfig& config, uint32_t* idOut) {
  *idOut = nextId;
  return config.isNative ? CreateNativeObject(config) : CreateProxy(config);
}

JSObject* CreateNativeObject(const ObjectConfig& config) {
  MOZ_ASSERT(config.isNative);

  RootedNativeObject obj(
      cx, NewBuiltinClassInstance(cx, config.clasp, TenuredObject));
  if (!obj) {
    return nullptr;
  }

  for (uint32_t i = 0; i < JSCLASS_RESERVED_SLOTS(config.clasp); i++) {
    JS::SetReservedSlot(obj, i, Int32Value(nextId++));
  }

  if (config.native.inDictionaryMode) {
    // Put object in dictionary mode by defining a non-last property and
    // deleting it later.
    MOZ_RELEASE_ASSERT(config.native.propCount != 0);
    if (!JS_DefineProperty(cx, obj, "dummy", 0, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }

  for (uint32_t i = 0; i < config.native.propCount; i++) {
    RootedId name(cx, CreatePropName(nextId++));
    if (name.isVoid()) {
      return nullptr;
    }

    if (!JS_DefinePropertyById(cx, obj, name, nextId++, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }

  if (config.native.inDictionaryMode) {
    JS::ObjectOpResult result;
    JS_DeleteProperty(cx, obj, "dummy", result);
    MOZ_RELEASE_ASSERT(result.ok());
  }

  MOZ_RELEASE_ASSERT(obj->inDictionaryMode() == config.native.inDictionaryMode);

  return obj;
}

JSObject* CreateProxy(const ObjectConfig& config) {
  RootedValue priv(cx, Int32Value(nextId++));

  RootedObject expando(cx, NewPlainObject(cx));
  RootedValue expandoId(cx, Int32Value(nextId++));
  if (!expando || !JS_SetProperty(cx, expando, "id", expandoId)) {
    return nullptr;
  }

  ProxyOptions options;
  options.setClass(config.clasp);
  options.setLazyProto(true);

  RootedObject obj(cx, NewProxyObject(cx, &DummyProxyHandler::singleton, priv,
                                      nullptr, options));
  if (!obj) {
    return nullptr;
  }

  Rooted<ProxyObject*> proxy(cx, &obj->as<ProxyObject>());
  proxy->setExpando(expando);

  for (uint32_t i = 0; i < JSCLASS_RESERVED_SLOTS(config.clasp); i++) {
    JS::SetReservedSlot(proxy, i, Int32Value(nextId++));
  }

  if (!config.proxy.inlineValues) {
    // To create a proxy with non-inline values we must swap the proxy with an
    // object with a different size.
    RootedObject dummy(
        cx, NewBuiltinClassInstance(cx, &TestDOMClasses[0], TenuredObject));
    if (!dummy) {
      return nullptr;
    }

    AutoEnterOOMUnsafeRegion oomUnsafe;
    JSObject::swap(cx, obj, dummy, oomUnsafe);
    proxy = &dummy->as<ProxyObject>();
  }

  MOZ_RELEASE_ASSERT(proxy->usingInlineValueArray() ==
                     config.proxy.inlineValues);

  return proxy;
}

bool CheckObject(HandleObject obj, const ObjectConfig& config, uint32_t id) {
  CHECK(obj->is<NativeObject>() == config.isNative);
  CHECK(obj->getClass() == config.clasp);

  uint32_t reservedSlots = JSCLASS_RESERVED_SLOTS(config.clasp);

  if (Verbose) {
    fprintf(stderr, "Check %p is a %s object with %u reserved slots", obj.get(),
            config.isNative ? "native" : "proxy", reservedSlots);
    if (config.isNative) {
      fprintf(stderr, ", %u properties and %s in dictionary mode",
              config.native.propCount,
              config.native.inDictionaryMode ? "is" : "is not");
    } else {
      fprintf(stderr, " with %s values",
              config.proxy.inlineValues ? "inline" : "out-of-line");
    }
    fprintf(stderr, "\n");
  }

  if (!config.isNative) {
    CHECK(GetProxyPrivate(obj) == Int32Value(id++));

    Value expandoValue = GetProxyExpando(obj);
    CHECK(expandoValue.isObject());

    RootedObject expando(cx, &expandoValue.toObject());
    RootedValue expandoId(cx);
    JS_GetProperty(cx, expando, "id", &expandoId);
    CHECK(expandoId == Int32Value(id++));
  }

  for (uint32_t i = 0; i < reservedSlots; i++) {
    CHECK(JS::GetReservedSlot(obj, i) == Int32Value(id++));
  }

  if (config.isNative) {
    NativeObject* nobj = &obj->as<NativeObject>();
    uint32_t propCount = GetPropertyCount(nobj);

    CHECK(propCount == config.native.propCount);

    for (uint32_t i = 0; i < config.native.propCount; i++) {
      RootedId name(cx, CreatePropName(id++));
      CHECK(!name.isVoid());

      RootedValue value(cx);
      CHECK(JS_GetPropertyById(cx, obj, name, &value));
      CHECK(value == Int32Value(id++));
    }
  }

  return true;
}

uint32_t GetPropertyCount(NativeObject* obj) {
  uint32_t count = 0;
  for (ShapePropertyIter<NoGC> iter(obj->shape()); !iter.done(); iter++) {
    count++;
  }

  return count;
}

jsid CreatePropName(uint32_t id) {
  char buffer[32];
  sprintf(buffer, "prop%u", id);

  RootedString atom(cx, JS_AtomizeString(cx, buffer));
  if (!atom) {
    return jsid::Void();
  }

  return jsid::NonIntAtom(atom);
}

END_TEST(testObjectSwap)
