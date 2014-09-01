/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptLogging__
#define mozilla_jsipc_JavaScriptLogging__

#include "nsString.h"
#include "nsPrintfCString.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"

namespace mozilla {
namespace jsipc {

#define LOG(...)						               \
    PR_BEGIN_MACRO                                                             \
    if (LoggingEnabled()) {                                                    \
	Logging log(this, cx);					               \
	log.print(__VA_ARGS__);					               \
    }                                                                          \
    PR_END_MACRO

#define LOG_STACK()		                                               \
    PR_BEGIN_MACRO                                                             \
    if (StackLoggingEnabled()) {                                               \
        js_DumpBacktrace(cx);	                                               \
    }                                                                          \
    PR_END_MACRO

struct ReceiverObj
{
    ObjectId id;
    explicit ReceiverObj(ObjectId id) : id(id) {}
};

struct InVariant
{
    JSVariant variant;
    explicit InVariant(const JSVariant &variant) : variant(variant) {}
};

struct OutVariant
{
    JSVariant variant;
    explicit OutVariant(const JSVariant &variant) : variant(variant) {}
};

class Logging
{
  public:
    Logging(JavaScriptShared *shared, JSContext *cx) : shared(shared), cx(cx) {}

    void print(const nsCString &str) {
        const char *side = shared->isParent() ? "from child" : "from parent";
        printf("CPOW %s: %s\n", side, str.get());
    }

    void print(const char *str) {
        print(nsCString(str));
    }
    template<typename T1>
    void print(const char *fmt, const T1 &a1) {
        nsAutoCString tmp1;
        format(a1, tmp1);
        print(nsPrintfCString(fmt, tmp1.get()));
    }
    template<typename T1, typename T2>
    void print(const char *fmt, const T1 &a1, const T2 &a2) {
        nsAutoCString tmp1;
        nsAutoCString tmp2;
        format(a1, tmp1);
        format(a2, tmp2);
        print(nsPrintfCString(fmt, tmp1.get(), tmp2.get()));
    }
    template<typename T1, typename T2, typename T3>
    void print(const char *fmt, const T1 &a1, const T2 &a2, const T3 &a3) {
        nsAutoCString tmp1;
        nsAutoCString tmp2;
        nsAutoCString tmp3;
        format(a1, tmp1);
        format(a2, tmp2);
        format(a3, tmp3);
        print(nsPrintfCString(fmt, tmp1.get(), tmp2.get(), tmp3.get()));
    }

    void format(const nsString &str, nsCString &out) {
        out = NS_ConvertUTF16toUTF8(str);
    }

    void formatObject(bool incoming, bool local, ObjectId id, nsCString &out) {
        const char *side, *objDesc;

        if (local == incoming) {
            JS::RootedObject obj(cx);
            obj = shared->findObjectById(id);
            if (obj) {
                JSAutoCompartment ac(cx, obj);
                objDesc = js_ObjectClassName(cx, obj);
            } else {
                objDesc = "<dead object>";
            }

            side = shared->isParent() ? "parent" : "child";
        } else {
            objDesc = "<cpow>";
            side = shared->isParent() ? "child" : "parent";
        }

        out = nsPrintfCString("<%s %s:%d>", side, objDesc, id);
    }


    void format(const ReceiverObj &obj, nsCString &out) {
        formatObject(true, true, obj.id, out);
    }

    void format(const nsTArray<JSParam> &values, nsCString &out) {
        nsAutoCString tmp;
        out.Truncate();
        for (size_t i = 0; i < values.Length(); i++) {
            if (i)
                out.AppendLiteral(", ");
            if (values[i].type() == JSParam::Tvoid_t) {
                out.AppendLiteral("<void>");
            } else {
                format(InVariant(values[i].get_JSVariant()), tmp);
                out += tmp;
            }
        }
    }

    void format(const InVariant &value, nsCString &out) {
        format(true, value.variant, out);
    }

    void format(const OutVariant &value, nsCString &out) {
        format(false, value.variant, out);
    }

    void format(bool incoming, const JSVariant &value, nsCString &out) {
        switch (value.type()) {
          case JSVariant::TUndefinedVariant: {
              out = "undefined";
              break;
          }
          case JSVariant::TNullVariant: {
              out = "null";
              break;
          }
          case JSVariant::TnsString: {
              nsAutoCString tmp;
              format(value.get_nsString(), tmp);
              out = nsPrintfCString("\"%s\"", tmp.get());
              break;
          }
          case JSVariant::TObjectVariant: {
              const ObjectVariant &ovar = value.get_ObjectVariant();
              if (ovar.type() == ObjectVariant::TLocalObject)
                  formatObject(incoming, true, ovar.get_LocalObject().id(), out);
              else
                  formatObject(incoming, false, ovar.get_RemoteObject().id(), out);
              break;
          }
          case JSVariant::Tdouble: {
              out = nsPrintfCString("%.0f", value.get_double());
              break;
          }
          case JSVariant::Tbool: {
              out = value.get_bool() ? "true" : "false";
              break;
          }
          case JSVariant::TJSIID: {
              out = "<JSIID>";
              break;
          }
          default: {
              out = "<JSIID>";
              break;
          }
        }
    }

  private:
    JavaScriptShared *shared;
    JSContext *cx;
};

}
}

#endif
