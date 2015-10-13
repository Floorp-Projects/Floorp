/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/StructuredClone.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testStructuredClone_object)
{
    JS::RootedObject g1(cx, createGlobal());
    JS::RootedObject g2(cx, createGlobal());
    CHECK(g1);
    CHECK(g2);

    JS::RootedValue v1(cx);

    {
        JSAutoCompartment ac(cx, g1);
        JS::RootedValue prop(cx, JS::Int32Value(1337));

        JS::RootedObject obj(cx, JS_NewPlainObject(cx));
        v1 = JS::ObjectOrNullValue(obj);
        CHECK(v1.isObject());
        CHECK(JS_SetProperty(cx, obj, "prop", prop));
    }

    {
        JSAutoCompartment ac(cx, g2);
        JS::RootedValue v2(cx);

        CHECK(JS_StructuredClone(cx, v1, &v2, nullptr, nullptr));
        CHECK(v2.isObject());
        JS::RootedObject obj(cx, &v2.toObject());

        JS::RootedValue prop(cx);
        CHECK(JS_GetProperty(cx, obj, "prop", &prop));
        CHECK(prop.isInt32());
        CHECK(&v1.toObject() != obj);
        CHECK_EQUAL(prop.toInt32(), 1337);
    }

    return true;
}
END_TEST(testStructuredClone_object)

BEGIN_TEST(testStructuredClone_string)
{
    JS::RootedObject g1(cx, createGlobal());
    JS::RootedObject g2(cx, createGlobal());
    CHECK(g1);
    CHECK(g2);

    JS::RootedValue v1(cx);

    {
        JSAutoCompartment ac(cx, g1);
        JS::RootedValue prop(cx, JS::Int32Value(1337));

        v1 = JS::StringValue(JS_NewStringCopyZ(cx, "Hello World!"));
        CHECK(v1.isString());
        CHECK(v1.toString());
    }

    {
        JSAutoCompartment ac(cx, g2);
        JS::RootedValue v2(cx);

        CHECK(JS_StructuredClone(cx, v1, &v2, nullptr, nullptr));
        CHECK(v2.isString());
        CHECK(v2.toString());

        JS::RootedValue expected(cx, JS::StringValue(
            JS_NewStringCopyZ(cx, "Hello World!")));
        CHECK_SAME(v2, expected);
    }

    return true;
}
END_TEST(testStructuredClone_string)

struct StructuredCloneTestPrincipals final : public JSPrincipals {
    uint32_t rank;

    explicit StructuredCloneTestPrincipals(uint32_t rank, int32_t rc = 1) : rank(rank) {
        this->refcount = rc;
    }

    bool write(JSContext* cx, JSStructuredCloneWriter* writer) override {
        return JS_WriteUint32Pair(writer, rank, 0);
    }

    static bool read(JSContext* cx, JSStructuredCloneReader *reader, JSPrincipals** outPrincipals) {
        uint32_t rank;
        uint32_t unused;
        if (!JS_ReadUint32Pair(reader, &rank, &unused))
            return false;

        *outPrincipals = new StructuredCloneTestPrincipals(rank);
        return !!*outPrincipals;
    }

    static void destroy(JSPrincipals* p) {
        auto p1 = static_cast<StructuredCloneTestPrincipals*>(p);
        delete p1;
    }

    static uint32_t getRank(JSPrincipals* p) {
        if (!p)
            return 0;
        return static_cast<StructuredCloneTestPrincipals*>(p)->rank;
    }

    static bool subsumes(JSPrincipals* a, JSPrincipals* b) {
        return getRank(a) > getRank(b);
    }

    static JSSecurityCallbacks securityCallbacks;

    static StructuredCloneTestPrincipals testPrincipals;
};

JSSecurityCallbacks StructuredCloneTestPrincipals::securityCallbacks = {
    nullptr, // contentSecurityPolicyAllows
    subsumes
};

BEGIN_TEST(testStructuredClone_SavedFrame)
{
    JS_SetSecurityCallbacks(rt, &StructuredCloneTestPrincipals::securityCallbacks);
    JS_InitDestroyPrincipalsCallback(rt, StructuredCloneTestPrincipals::destroy);
    JS_InitReadPrincipalsCallback(rt, StructuredCloneTestPrincipals::read);

    auto testPrincipals = new StructuredCloneTestPrincipals(42, 0);
    CHECK(testPrincipals);

    auto DONE = (JSPrincipals*) 0xDEADBEEF;

    struct {
        const char* name;
        JSPrincipals* principals;
    } principalsToTest[] = {
        { "IsSystem", &js::ReconstructedSavedFramePrincipals::IsSystem },
        { "IsNotSystem", &js::ReconstructedSavedFramePrincipals::IsNotSystem },
        { "testPrincipals", testPrincipals },
        { "nullptr principals", nullptr },
        { "DONE", DONE }
    };

    const char* FILENAME = "filename.js";

    for (auto* pp = principalsToTest; pp->principals != DONE; pp++) {
        fprintf(stderr, "Testing with principals '%s'\n", pp->name);

        JS::RootedObject g(cx, JS_NewGlobalObject(cx, getGlobalClass(), pp->principals,
                                                  JS::FireOnNewGlobalHook));
        CHECK(g);
        JSAutoCompartment ac(cx, g);

        CHECK(js::DefineTestingFunctions(cx, g, false, false));

        JS::RootedValue srcVal(cx);
        CHECK(evaluate("(function one() {                      \n"  // 1
                       "  return (function two() {             \n"  // 2
                       "    return (function three() {         \n"  // 3
                       "      return saveStack();              \n"  // 4
                       "    }());                              \n"  // 5
                       "  }());                                \n"  // 6
                       "}());                                  \n", // 7
                       FILENAME,
                       1,
                       &srcVal));

        CHECK(srcVal.isObject());
        JS::RootedObject srcObj(cx, &srcVal.toObject());

        CHECK(srcObj->is<js::SavedFrame>());
        js::RootedSavedFrame srcFrame(cx, &srcObj->as<js::SavedFrame>());

        CHECK(srcFrame->getPrincipals() == pp->principals);

        JS::RootedValue destVal(cx);
        CHECK(JS_StructuredClone(cx, srcVal, &destVal, nullptr, nullptr));

        CHECK(destVal.isObject());
        JS::RootedObject destObj(cx, &destVal.toObject());

        CHECK(destObj->is<js::SavedFrame>());
        auto destFrame = &destObj->as<js::SavedFrame>();

        size_t framesCopied = 0;
        for (auto& f : *destFrame) {
            framesCopied++;

            CHECK(&f != srcFrame);

            if (pp->principals == testPrincipals) {
                // We shouldn't get a pointer to the same
                // StructuredCloneTestPrincipals instance since we should have
                // serialized and then deserialized it into a new instance.
                CHECK(f.getPrincipals() != pp->principals);

                // But it should certainly have the same rank.
                CHECK(StructuredCloneTestPrincipals::getRank(f.getPrincipals()) ==
                      StructuredCloneTestPrincipals::getRank(pp->principals));
            } else {
                // For our singleton principals, we should always get the same
                // pointer back.
                CHECK(js::ReconstructedSavedFramePrincipals::is(pp->principals) ||
                      pp->principals == nullptr);
                CHECK(f.getPrincipals() == pp->principals);
            }

            CHECK(EqualStrings(f.getSource(), srcFrame->getSource()));
            CHECK(f.getLine() == srcFrame->getLine());
            CHECK(f.getColumn() == srcFrame->getColumn());
            CHECK(EqualStrings(f.getFunctionDisplayName(), srcFrame->getFunctionDisplayName()));

            srcFrame = srcFrame->getParent();
        }

        // Four function frames + one global frame.
        CHECK(framesCopied == 4);
    }

    return true;
}
END_TEST(testStructuredClone_SavedFrame)
