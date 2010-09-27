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
#include "nsXULAppAPI.h"
#include "nsNativeCharsetUtils.h"

#include "mozilla/jetpack/JetpackChild.h"
#include "mozilla/jetpack/Handle.h"

#include "jsarray.h"

#include <stdio.h>

namespace mozilla {
namespace jetpack {

JetpackChild::JetpackChild()
{
}

JetpackChild::~JetpackChild()
{
}

#define IMPL_METHOD_FLAGS (JSPROP_ENUMERATE | \
                           JSPROP_READONLY | \
                           JSPROP_PERMANENT)
const JSFunctionSpec
JetpackChild::sImplMethods[] = {
  JS_FN("sendMessage", SendMessage, 3, IMPL_METHOD_FLAGS),
  JS_FN("callMessage", CallMessage, 2, IMPL_METHOD_FLAGS),
  JS_FN("registerReceiver", RegisterReceiver, 2, IMPL_METHOD_FLAGS),
  JS_FN("unregisterReceiver", UnregisterReceiver, 2, IMPL_METHOD_FLAGS),
  JS_FN("unregisterReceivers", UnregisterReceivers, 1, IMPL_METHOD_FLAGS),
  JS_FN("createHandle", CreateHandle, 0, IMPL_METHOD_FLAGS),
  JS_FN("createSandbox", CreateSandbox, 0, IMPL_METHOD_FLAGS),
  JS_FN("evalInSandbox", EvalInSandbox, 2, IMPL_METHOD_FLAGS),
  JS_FN("gc", GC, 0, IMPL_METHOD_FLAGS),
#ifdef JS_GC_ZEAL
  JS_FN("gczeal", GCZeal, 1, IMPL_METHOD_FLAGS),
#endif
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

#ifdef BUILD_CTYPES
static char*
UnicodeToNative(JSContext *cx, const jschar *source, size_t slen)
{
  nsCAutoString native;
  nsDependentString unicode(reinterpret_cast<const PRUnichar*>(source), slen);
  nsresult rv = NS_CopyUnicodeToNative(unicode, native);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "could not convert string to native charset");
    return NULL;
  }

  char* result = static_cast<char*>(JS_malloc(cx, native.Length() + 1));
  if (!result)
    return NULL;

  memcpy(result, native.get(), native.Length() + 1);
  return result;
}

static JSCTypesCallbacks sCallbacks = {
  UnicodeToNative
};
#endif

bool
JetpackChild::Init(base::ProcessHandle aParentProcessHandle,
                   MessageLoop* aIOLoop,
                   IPC::Channel* aChannel)
{
  if (!Open(aChannel, aParentProcessHandle, aIOLoop))
    return false;

  if (!(mRuntime = JS_NewRuntime(32L * 1024L * 1024L)) ||
      !(mCx = JS_NewContext(mRuntime, 8192)))
    return false;

  JS_SetVersion(mCx, JSVERSION_LATEST);
  JS_SetOptions(mCx, JS_GetOptions(mCx) |
                JSOPTION_DONT_REPORT_UNCAUGHT |
                JSOPTION_ATLINE |
                JSOPTION_JIT);
  JS_SetErrorReporter(mCx, ReportError);

  {
    JSAutoRequest request(mCx);
    JS_SetContextPrivate(mCx, this);
    JSObject* implGlobal =
      JS_NewCompartmentAndGlobalObject(mCx, const_cast<JSClass*>(&sGlobalClass), NULL);
    jsval ctypes;
    if (!implGlobal ||
        !JS_InitStandardClasses(mCx, implGlobal) ||
#ifdef BUILD_CTYPES
        !JS_InitCTypesClass(mCx, implGlobal) ||
        !JS_GetProperty(mCx, implGlobal, "ctypes", &ctypes) ||
        !JS_SetCTypesCallbacks(mCx, JSVAL_TO_OBJECT(ctypes), &sCallbacks) ||
#endif
        !JS_DefineFunctions(mCx, implGlobal,
                            const_cast<JSFunctionSpec*>(sImplMethods)))
      return false;
  }

  return true;
}

void
JetpackChild::CleanUp()
{
  ClearReceivers();
  JS_DestroyContext(mCx);
  JS_DestroyRuntime(mRuntime);
  JS_ShutDown();
}

void
JetpackChild::ActorDestroy(ActorDestroyReason why)
{
  XRE_ShutdownChildProcess();
}

bool
JetpackChild::RecvSendMessage(const nsString& messageName,
                              const nsTArray<Variant>& data)
{
  JSAutoRequest request(mCx);
  return JetpackActorCommon::RecvMessage(mCx, messageName, data, NULL);
}

bool
JetpackChild::RecvEvalScript(const nsString& code)
{
  JSAutoRequest request(mCx);

  js::AutoValueRooter ignored(mCx);
  (void) JS_EvaluateUCScript(mCx, JS_GetGlobalObject(mCx), code.get(),
                             code.Length(), "", 1, ignored.jsval_addr());
  return true;
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
  JS_ASSERT(cx == self->mCx);
  return self;
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
  if (!GetThis(cx)->CallCallMessage(smr.msgName, smr.data, &results)) {
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

JSBool
JetpackChild::CreateSandbox(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc > 0) {
    JS_ReportError(cx, "createSandbox takes zero arguments");
    return JS_FALSE;
  }

  JSObject* obj = JS_NewCompartmentAndGlobalObject(cx, const_cast<JSClass*>(&sGlobalClass), NULL);
  if (!obj)
    return JS_FALSE;

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, obj))
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
  return JS_InitStandardClasses(cx, obj);
}

JSBool
JetpackChild::EvalInSandbox(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 2) {
    JS_ReportError(cx, "evalInSandbox takes two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);

  JSObject* obj;
  if (!JSVAL_IS_OBJECT(argv[0]) ||
      !(obj = JSVAL_TO_OBJECT(argv[0])) ||
      &sGlobalClass != JS_GetClass(cx, obj) ||
      obj == JS_GetGlobalObject(cx)) {
    JS_ReportError(cx, "The first argument to evalInSandbox must be a global object created using createSandbox.");
    return JS_FALSE;
  }

  JSString* str = JS_ValueToString(cx, argv[1]);
  if (!str)
    return JS_FALSE;

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, obj))
    return JS_FALSE;

  js::AutoValueRooter ignored(cx);
  return JS_EvaluateUCScript(cx, obj, JS_GetStringChars(str), JS_GetStringLength(str), "", 1,
                             ignored.jsval_addr());
}

bool JetpackChild::sReportingError;

/* static */ void
JetpackChild::ReportError(JSContext* cx, const char* message,
                          JSErrorReport* report)
{
  if (sReportingError) {
    NS_WARNING("Recursive error reported.");
    return;
  }

  sReportingError = true;

  js::AutoObjectRooter obj(cx, JS_NewObject(cx, NULL, NULL, NULL));

  if (report && report->filename) {
    jsval filename = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, report->filename));
    JS_SetProperty(cx, obj.object(), "fileName", &filename);
  }

  if (report) {
    jsval lineno = INT_TO_JSVAL(report->lineno);
    JS_SetProperty(cx, obj.object(), "lineNumber", &lineno);
  }

  jsval msgstr = JSVAL_NULL;
  if (report && report->ucmessage)
    msgstr = STRING_TO_JSVAL(JS_NewUCStringCopyZ(cx, report->ucmessage));
  else
    msgstr = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, message));
  JS_SetProperty(cx, obj.object(), "message", &msgstr);

  MessageResult smr;
  Variant* vp = smr.data.AppendElement();
  JetpackActorCommon::jsval_to_Variant(cx, OBJECT_TO_JSVAL(obj.object()), vp);
  GetThis(cx)->SendSendMessage(NS_LITERAL_STRING("core:exception"), smr.data);

  sReportingError = false;
}

JSBool
JetpackChild::GC(JSContext* cx, uintN argc, jsval *vp)
{
  JS_GC(cx);
  return JS_TRUE;
}

#ifdef JS_GC_ZEAL
JSBool
JetpackChild::GCZeal(JSContext* cx, uintN argc, jsval *vp)
{
  jsval* argv = JS_ARGV(cx, vp);

  uint32 zeal;
  if (!JS_ValueToECMAUint32(cx, argv[0], &zeal))
    return JS_FALSE;

  JS_SetGCZeal(cx, PRUint8(zeal));
  return JS_TRUE;
}
#endif

} // namespace jetpack
} // namespace mozilla
