/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "base/basictypes.h"
#include "jscntxt.h"

#include "mozilla/jetpack/JetpackChild.h"
#include "mozilla/jetpack/Handle.h"

#include "jsarray.h"

namespace mozilla {
namespace jetpack {

JetpackChild::JetpackChild()
{
}

JetpackChild::~JetpackChild()
{
}

#define IMPL_PROP_FLAGS (JSPROP_SHARED | \
                         JSPROP_ENUMERATE | \
                         JSPROP_READONLY | \
                         JSPROP_PERMANENT)
const JSPropertySpec
JetpackChild::sImplProperties[] = {
  { "jetpack", 0, IMPL_PROP_FLAGS, UserJetpackGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

#undef IMPL_PROP_FLAGS

#define IMPL_METHOD_FLAGS (JSFUN_FAST_NATIVE |  \
                           JSPROP_ENUMERATE | \
                           JSPROP_READONLY | \
                           JSPROP_PERMANENT)
const JSFunctionSpec
JetpackChild::sImplMethods[] = {
  JS_FN("sendMessage", SendMessage, 3, IMPL_METHOD_FLAGS),
  JS_FN("callMessage", CallMessage, 2, IMPL_METHOD_FLAGS),
  JS_FN("registerReceiver", RegisterReceiver, 2, IMPL_METHOD_FLAGS),
  JS_FN("unregisterReceiver", UnregisterReceiver, 2, IMPL_METHOD_FLAGS),
  JS_FN("unregisterReceivers", UnregisterReceivers, 1, IMPL_METHOD_FLAGS),
  JS_FN("wrap", Wrap, 1, IMPL_METHOD_FLAGS),
  JS_FN("createHandle", CreateHandle, 0, IMPL_METHOD_FLAGS),
  JS_FS_END
};

#undef IMPL_METHOD_FLAGS

const JSClass
JetpackChild::sGlobalClass = {
  "JetpackChild::sGlobalClass", JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};
  
bool
JetpackChild::Init(base::ProcessHandle aParentProcessHandle,
                   MessageLoop* aIOLoop,
                   IPC::Channel* aChannel)
{
  if (!Open(aChannel, aParentProcessHandle, aIOLoop))
    return false;

  if (!(mRuntime = JS_NewRuntime(32L * 1024L * 1024L)) ||
      !(mImplCx = JS_NewContext(mRuntime, 8192)) ||
      !(mUserCx = JS_NewContext(mRuntime, 8192)))
    return false;

  {
    JSAutoRequest request(mImplCx);
    JS_SetContextPrivate(mImplCx, this);
    JSObject* implGlobal =
      JS_NewGlobalObject(mImplCx, const_cast<JSClass*>(&sGlobalClass));
    if (!implGlobal ||
        !JS_InitStandardClasses(mImplCx, implGlobal) ||
        !JS_DefineProperties(mImplCx, implGlobal,
                             const_cast<JSPropertySpec*>(sImplProperties)) ||
        !JS_DefineFunctions(mImplCx, implGlobal,
                            const_cast<JSFunctionSpec*>(sImplMethods)))
      return false;
  }

  {
    JSAutoRequest request(mUserCx);
    JS_SetContextPrivate(mUserCx, this);
    JSObject* userGlobal =
      JS_NewGlobalObject(mUserCx, const_cast<JSClass*>(&sGlobalClass));
    if (!userGlobal ||
        !JS_InitStandardClasses(mUserCx, userGlobal))
      return false;
  }

  return true;
}

void
JetpackChild::CleanUp()
{
  JS_DestroyContext(mUserCx);
  JS_DestroyContext(mImplCx);
  JS_DestroyRuntime(mRuntime);
  JS_ShutDown();
}

bool
JetpackChild::RecvSendMessage(const nsString& messageName,
                              const nsTArray<Variant>& data)
{
  JSAutoRequest request(mImplCx);
  return JetpackActorCommon::RecvMessage(mImplCx, messageName, data, NULL);
}

static bool
Evaluate(JSContext* cx, const nsCString& code)
{
  JSAutoRequest request(cx);
  js::AutoValueRooter ignored(cx);
  JS_EvaluateScript(cx, JS_GetGlobalObject(cx), code.get(),
                    code.Length(), "", 1, ignored.addr());
  return true;
}

bool
JetpackChild::RecvLoadImplementation(const nsCString& code)
{
  return Evaluate(mImplCx, code);
}

bool
JetpackChild::RecvLoadUserScript(const nsCString& code)
{
  return Evaluate(mUserCx, code);
}

PHandleChild*
JetpackChild::AllocPHandle()
{
  return new HandleChild();
}

bool
JetpackChild::DeallocPHandle(PHandleChild* actor)
{
  delete actor;
  return true;
}

JetpackChild*
JetpackChild::GetThis(JSContext* cx)
{
  JetpackChild* self =
    static_cast<JetpackChild*>(JS_GetContextPrivate(cx));
  JS_ASSERT(cx == self->mImplCx ||
            cx == self->mUserCx);
  return self;
}

JSBool
JetpackChild::UserJetpackGetter(JSContext* cx, JSObject* obj, jsval id,
                                jsval* vp)
{
  JSObject* userGlobal = JS_GetGlobalObject(GetThis(cx)->mUserCx);
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(userGlobal));
  return JS_TRUE;
}

struct MessageResult {
  nsString msgName;
  nsTArray<Variant> data;
};

static JSBool
MessageCommon(JSContext* cx, uintN argc, jsval* vp,
              MessageResult* result)
{
  if (argc < 1) {
    JS_ReportError(cx, "Message requires a name, at least");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);

  JSString* msgNameStr = JS_ValueToString(cx, argv[0]);
  if (!msgNameStr) {
    JS_ReportError(cx, "Could not convert value to string");
    return JS_FALSE;
  }

  result->msgName.Assign((PRUnichar*)JS_GetStringChars(msgNameStr),
                         JS_GetStringLength(msgNameStr));

  result->data.Clear();

  if (!result->data.SetCapacity(argc)) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  for (uintN i = 1; i < argc; ++i) {
    Variant* vp = result->data.AppendElement();
    if (!JetpackActorCommon::jsval_to_Variant(cx, argv[i], vp)) {
      JS_ReportError(cx, "Invalid message argument at position %d", i);
      return JS_FALSE;
    }
  }

  return JS_TRUE;
}

JSBool
JetpackChild::SendMessage(JSContext* cx, uintN argc, jsval* vp)
{
  MessageResult smr;
  if (!MessageCommon(cx, argc, vp, &smr))
    return JS_FALSE;

  if (!GetThis(cx)->SendSendMessage(smr.msgName, smr.data)) {
    JS_ReportError(cx, "Failed to sendMessage");
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
JetpackChild::CallMessage(JSContext* cx, uintN argc, jsval* vp)
{
  MessageResult smr;
  if (!MessageCommon(cx, argc, vp, &smr))
    return JS_FALSE;

  nsTArray<Variant> results;
  if (!GetThis(cx)->SendCallMessage(smr.msgName, smr.data, &results)) {
    JS_ReportError(cx, "Failed to callMessage");
    return JS_FALSE;
  }

  nsAutoTArray<jsval, 4> jsvals;
  jsval* rvals = jsvals.AppendElements(results.Length());
  if (!rvals) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  for (PRUint32 i = 0; i < results.Length(); ++i)
    rvals[i] = JSVAL_VOID;
  js::AutoArrayRooter root(cx, results.Length(), rvals);

  for (PRUint32 i = 0; i < results.Length(); ++i)
    if (!jsval_from_Variant(cx, results.ElementAt(i), rvals + i)) {
      JS_ReportError(cx, "Invalid result from handler %d", i);
      return JS_FALSE;
    }

  JSObject* arrObj = JS_NewArrayObject(cx, results.Length(), rvals);
  if (!arrObj) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(arrObj));

  return JS_TRUE;
}

struct ReceiverResult
{
  nsString msgName;
  jsval receiver;
};

static JSBool
ReceiverCommon(JSContext* cx, uintN argc, jsval* vp,
               const char* methodName, uintN arity,
               ReceiverResult* result)
{
  if (argc != arity) {
    JS_ReportError(cx, "%s requires exactly %d arguments", methodName, arity);
    return JS_FALSE;
  }

  // Not currently possible, but think of the future.
  if (arity < 1)
    return JS_TRUE;

  jsval* argv = JS_ARGV(cx, vp);

  JSString* str = JS_ValueToString(cx, argv[0]);
  if (!str) {
    JS_ReportError(cx, "%s expects a stringifiable value as its first argument",
                   methodName);
    return JS_FALSE;
  }

  result->msgName.Assign((PRUnichar*)JS_GetStringChars(str),
                         JS_GetStringLength(str));

  if (arity < 2)
    return JS_TRUE;

  if (!JSVAL_IS_OBJECT(argv[1]) ||
      !JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(argv[1])))
  {
    JS_ReportError(cx, "%s expects a function as its second argument",
                   methodName);
    return JS_FALSE;
  }

  // GC-safe because argv is rooted.
  result->receiver = argv[1];

  return JS_TRUE;
}

JSBool
JetpackChild::RegisterReceiver(JSContext* cx, uintN argc, jsval* vp)
{
  ReceiverResult rr;
  if (!ReceiverCommon(cx, argc, vp, "registerReceiver", 2, &rr))
    return JS_FALSE;

  JetpackActorCommon* actor = GetThis(cx);
  nsresult rv = actor->RegisterReceiver(cx, rr.msgName, rr.receiver);
  if (NS_FAILED(rv)) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
JetpackChild::UnregisterReceiver(JSContext* cx, uintN argc, jsval* vp)
{
  ReceiverResult rr;
  if (!ReceiverCommon(cx, argc, vp, "unregisterReceiver", 2, &rr))
    return JS_FALSE;

  JetpackActorCommon* actor = GetThis(cx);
  actor->UnregisterReceiver(rr.msgName, rr.receiver);
  return JS_TRUE;
}

JSBool
JetpackChild::UnregisterReceivers(JSContext* cx, uintN argc, jsval* vp)
{
  ReceiverResult rr;
  if (!ReceiverCommon(cx, argc, vp, "unregisterReceivers", 1, &rr))
    return JS_FALSE;

  JetpackActorCommon* actor = GetThis(cx);
  actor->UnregisterReceivers(rr.msgName);
  return JS_TRUE;
}

JSBool
JetpackChild::Wrap(JSContext* cx, uintN argc, jsval* vp)
{
  NS_NOTYETIMPLEMENTED("wrap not yet implemented (depends on bug 563010)");
  return JS_FALSE;
}

JSBool
JetpackChild::CreateHandle(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc > 0) {
    JS_ReportError(cx, "createHandle takes zero arguments");
    return JS_FALSE;
  }

  HandleChild* handle;
  JSObject* hobj;

  PHandleChild* phc = GetThis(cx)->SendPHandleConstructor();
  if (!(handle = static_cast<HandleChild*>(phc)) ||
      !(hobj = handle->ToJSObject(cx))) {
    JS_ReportError(cx, "Failed to construct Handle");
    return JS_FALSE;
  }

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(hobj));

  return JS_TRUE;
}

} // namespace jetpack
} // namespace mozilla
