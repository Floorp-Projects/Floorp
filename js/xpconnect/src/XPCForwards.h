/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private forward declarations. */

#ifndef xpcforwards_h___
#define xpcforwards_h___

// forward declarations of internally used classes...

class nsXPConnect;
class XPCJSContext;
class XPCJSRuntime;
class XPCContext;
class XPCCallContext;

class XPCJSThrower;

class nsXPCWrappedJS;

class XPCNativeMember;
class XPCNativeInterface;
class XPCNativeSet;

class XPCWrappedNative;
class XPCWrappedNativeProto;
class XPCWrappedNativeTearOff;

class JSObject2WrappedJSMap;
class Native2WrappedNativeMap;
class IID2NativeInterfaceMap;
class ClassInfo2NativeSetMap;
class ClassInfo2WrappedNativeProtoMap;
class NativeSetMap;
class JSObject2JSObjectMap;

class nsXPCComponents;
class nsXPCComponents_Interfaces;
class nsXPCComponents_Classes;
class nsXPCComponents_Results;
class nsXPCComponents_ID;
class nsXPCComponents_Exception;
class nsXPCComponents_Constructor;
class nsXPCComponents_Utils;

class AutoMarkingPtr;

#endif /* xpcforwards_h___ */
