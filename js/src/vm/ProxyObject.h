/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ProxyObject_h
#define vm_ProxyObject_h

#include "jsobj.h"
#include "jsproxy.h"

namespace js {

// This is the base class for the various kinds of proxy objects.  It's never
// instantiated.
class ProxyObject : public JSObject
{
};

class FunctionProxyObject : public ProxyObject
{
  public:
    static Class class_;
};

class ObjectProxyObject : public ProxyObject
{
  public:
    static Class class_;
};

class OuterWindowProxyObject : public ObjectProxyObject
{
  public:
    static Class class_;
};

} // namespace js

// Note: the following |JSObject::is<T>| methods are implemented in terms of
// the Is*Proxy() friend API functions to ensure the implementations are tied
// together.  The exception is |JSObject::is<js::OuterWindowProxyObject>()
// const|, which uses the standard template definition, because there is no
// IsOuterWindowProxy() function in the friend API.

template<>
inline bool
JSObject::is<js::ProxyObject>() const
{
    return js::IsProxy(const_cast<JSObject*>(this));
}

template<>
inline bool
JSObject::is<js::FunctionProxyObject>() const
{
    return js::IsFunctionProxy(const_cast<JSObject*>(this));
}

// WARNING: This function succeeds for ObjectProxyObject *and*
// OuterWindowProxyObject (which is a sub-class).  If you want a test that only
// succeeds for ObjectProxyObject, use |hasClass(&ObjectProxyObject::class_)|.
template<>
inline bool
JSObject::is<js::ObjectProxyObject>() const
{
    return js::IsObjectProxy(const_cast<JSObject*>(this));
}

#endif /* vm_ProxyObject_h */
