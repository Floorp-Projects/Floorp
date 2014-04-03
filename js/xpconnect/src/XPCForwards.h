/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private forward declarations. */

#ifndef xpcforwards_h___
#define xpcforwards_h___

// forward declarations of interally used classes...

class nsXPConnect;
class XPCJSRuntime;
class XPCContext;
class XPCCallContext;

class XPCJSThrower;

class nsXPCWrappedJS;
class nsXPCWrappedJSClass;

class XPCNativeMember;
class XPCNativeInterface;
class XPCNativeSet;

class XPCWrappedNative;
class XPCWrappedNativeProto;
class XPCWrappedNativeTearOff;
class XPCNativeScriptableShared;
class XPCNativeScriptableInfo;
class XPCNativeScriptableCreateInfo;

class XPCTraceableVariant;
class XPCJSObjectHolder;

class JSObject2WrappedJSMap;
class Native2WrappedNativeMap;
class IID2WrappedJSClassMap;
class IID2NativeInterfaceMap;
class ClassInfo2NativeSetMap;
class ClassInfo2WrappedNativeProtoMap;
class NativeSetMap;
class IID2ThisTranslatorMap;
class XPCNativeScriptableSharedMap;
class XPCWrappedNativeProtoMap;
class JSObject2JSObjectMap;

class nsXPCComponents;
class nsXPCComponents_Interfaces;
class nsXPCComponents_InterfacesByID;
class nsXPCComponents_Classes;
class nsXPCComponents_ClassesByID;
class nsXPCComponents_Results;
class nsXPCComponents_ID;
class nsXPCComponents_Exception;
class nsXPCComponents_Constructor;
class nsXPCComponents_Utils;
class nsXPCConstructor;

class AutoMarkingPtr;

class xpcProperty;

#endif /* xpcforwards_h___ */
