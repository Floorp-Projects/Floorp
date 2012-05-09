/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code, released
 * June 24, 2010.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Andreas Gal <gal@mozilla.com>
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

#ifndef dombindings_h
#define dombindings_h

#include "jsapi.h"
#include "jsproxy.h"
#include "xpcpublic.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace binding {

class ProxyHandler : public js::ProxyHandler {
protected:
    ProxyHandler() : js::ProxyHandler(ProxyFamily())
    {
    }

public:
    virtual bool isInstanceOf(JSObject *prototype) = 0;
};

class NoType;
class NoOp {
public:
    typedef NoType* T;
    enum {
        hasOp = 0
    };
};

template<typename Type>
class Op {
public:
    typedef Type T;
    enum {
        hasOp = 1
    };
};

template<typename Type>
class Getter : public Op<Type>
{
};

template<typename Type>
class Setter : public Op<Type>
{
};

template<class Getter, class Setter=NoOp>
class Ops
{
public:
    typedef Getter G;
    typedef Setter S;
};

typedef Ops<NoOp, NoOp> NoOps;

template<class ListType, class Base, class IndexOps, class NameOps=NoOps>
class DerivedListClass {
public:
    typedef ListType LT;
    typedef Base B;
    typedef IndexOps IO;
    typedef NameOps NO;
};

class NoBase {
public:
    static JSObject *getPrototype(JSContext *cx, XPCWrappedNativeScope *scope,
                                  JSObject *receiver);
    static bool shouldCacheProtoShape(JSContext *cx, JSObject *proto, bool *shouldCache)
    {
        *shouldCache = true;
        return true;
    }
    static bool resolveNativeName(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc)
    {
        return true;
    }
    static bool nativeGet(JSContext *cx, JSObject *proxy, JSObject *proto, jsid id, bool *found,
                          JS::Value *vp)
    {
        *found = false;
        return true;
    }
    static nsISupports* nativeToSupports(nsISupports* aNative)
    {
        return aNative;
    }
};

template<class ListType, class IndexOps, class NameOps=NoOps>
class ListClass : public DerivedListClass<ListType, NoBase, IndexOps, NameOps> {
};

template<class LC>
class ListBase : public ProxyHandler {
protected:
    typedef typename LC::LT ListType;
    typedef typename LC::B Base;
    typedef typename LC::IO::G::T IndexGetterType;
    typedef typename LC::IO::S::T IndexSetterType;
    typedef typename LC::NO::G::T NameGetterType;
    typedef typename LC::NO::S::T NameSetterType;
    enum {
        hasIndexGetter = LC::IO::G::hasOp,
        hasIndexSetter = LC::IO::S::hasOp,
        hasNameGetter = LC::NO::G::hasOp,
        hasNameSetter = LC::NO::S::hasOp
    };

private:
    friend void Register(nsScriptNameSpaceManager* aNameSpaceManager);

    static ListBase<LC> instance;

    static js::Class sInterfaceClass;

    struct Properties {
        jsid &id;
        JSPropertyOp getter;
        JSStrictPropertyOp setter;
    };
    struct Methods {
        jsid &id;
        JSNative native;
        unsigned nargs;
    };

    static Properties sProtoProperties[];
    static size_t sProtoPropertiesCount;
    static Methods sProtoMethods[];
    static size_t sProtoMethodsCount;

    static JSObject *ensureExpandoObject(JSContext *cx, JSObject *obj);

    static js::Shape *getProtoShape(JSObject *obj);
    static void setProtoShape(JSObject *obj, js::Shape *shape);

    static JSBool length_getter(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

    static inline bool getItemAt(ListType *list, uint32_t i, IndexGetterType &item);
    static inline bool setItemAt(JSContext *cx, ListType *list, uint32_t i, IndexSetterType item);

    static inline bool namedItem(JSContext *cx, JSObject *obj, jsval *name, NameGetterType &result,
                                 bool *hasResult);

    static inline bool getNamedItem(ListType *list, const nsAString& aName, NameGetterType &item);
    static inline bool setNamedItem(JSContext *cx, ListType *list, const nsAString& aName,
                                    NameSetterType item);

    static bool getPropertyOnPrototype(JSContext *cx, JSObject *proxy, jsid id, bool *found,
                                       JS::Value *vp);
    static bool hasPropertyOnPrototype(JSContext *cx, JSObject *proxy, jsid id);

public:
    static JSObject *create(JSContext *cx, JSObject *scope, ListType *list,
                            nsWrapperCache* cache, bool *triedToWrap);

    static JSObject *getPrototype(JSContext *cx, JSObject *receiver, bool *enabled);
    static bool DefineDOMInterface(JSContext *cx, JSObject *receiver, bool *enabled)
    {
        return !!getPrototype(cx, receiver, enabled);
    }

    bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                               JSPropertyDescriptor *desc);
    bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                  JSPropertyDescriptor *desc);
    bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                        JSPropertyDescriptor *desc);
    bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, JS::AutoIdVector &props);
    bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    bool enumerate(JSContext *cx, JSObject *proxy, JS::AutoIdVector &props);

    bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, JS::Value *vp);
    bool getElementIfPresent(JSContext *cx, JSObject *proxy, JSObject *receiver,
                             uint32_t index, JS::Value *vp, bool *present);
    bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
             JS::Value *vp);
    bool keys(JSContext *cx, JSObject *proxy, JS::AutoIdVector &props);
    bool iterate(JSContext *cx, JSObject *proxy, unsigned flags, JS::Value *vp);

    /* Spidermonkey extensions. */
    bool hasInstance(JSContext *cx, JSObject *proxy, const JS::Value *vp, bool *bp);
    JSString *obj_toString(JSContext *cx, JSObject *proxy);
    void finalize(JSFreeOp *fop, JSObject *proxy);

    static bool proxyHandlerIsList(js::ProxyHandler *handler) {
        return handler == &instance;
    }
    static bool objIsList(JSObject *obj) {
        return js::IsProxy(obj) && proxyHandlerIsList(js::GetProxyHandler(obj));
    }
    static inline bool instanceIsListObject(JSContext *cx, JSObject *obj, JSObject *callee);
    virtual bool isInstanceOf(JSObject *prototype)
    {
        return js::GetObjectClass(prototype) == &sInterfaceClass;
    }
    static inline ListType *getListObject(JSObject *obj);

    static JSObject *getPrototype(JSContext *cx, XPCWrappedNativeScope *scope,
                                  JSObject *receiver);
    static inline bool protoIsClean(JSContext *cx, JSObject *proto, bool *isClean);
    static bool shouldCacheProtoShape(JSContext *cx, JSObject *proto, bool *shouldCache);
    static bool resolveNativeName(JSContext *cx, JSObject *proxy, jsid id,
                                  JSPropertyDescriptor *desc);
    static bool nativeGet(JSContext *cx, JSObject *proxy, JSObject *proto, jsid id, bool *found,
                          JS::Value *vp);
    static ListType *getNative(JSObject *proxy);
    static nsISupports* nativeToSupports(ListType* aNative)
    {
        return Base::nativeToSupports(aNative);
    }
};

struct nsISupportsResult
{
    nsISupportsResult()
    {
    }
    nsISupports *mResult;
    nsWrapperCache *mCache;
};

}
}
}

#include "dombindings_gen.h"

#endif /* dombindings_h */
