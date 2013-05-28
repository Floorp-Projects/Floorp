/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNPAPIPlugin_h_
#define nsNPAPIPlugin_h_

#include "prlink.h"
#include "npfunctions.h"
#include "nsPluginHost.h"

#include "jsapi.h"
#include "nsCxPusher.h"

#include "mozilla/PluginLibrary.h"

/*
 * Use this macro before each exported function
 * (between the return address and the function
 * itself), to ensure that the function has the
 * right calling conventions on OS/2.
 */
#define NP_CALLBACK NP_LOADDS

#if defined(XP_WIN)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)
#elif defined(XP_OS2)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (_System * _name)
#else
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (* _name)
#endif

typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks, NPPluginFuncs* fCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) ();

class nsNPAPIPlugin : public nsISupports
{
private:
  typedef mozilla::PluginLibrary PluginLibrary;

public:
  nsNPAPIPlugin();
  virtual ~nsNPAPIPlugin();

  NS_DECL_ISUPPORTS

  // Constructs and initializes an nsNPAPIPlugin object. A NULL file path
  // will prevent this from calling NP_Initialize.
  static nsresult CreatePlugin(nsPluginTag *aPluginTag, nsNPAPIPlugin** aResult);

  PluginLibrary* GetLibrary();
  // PluginFuncs() can't fail but results are only valid if GetLibrary() succeeds
  NPPluginFuncs* PluginFuncs();

#if defined(XP_MACOSX) && !defined(__LP64__)
  void SetPluginRefNum(short aRefNum);
#endif

  // The IPC mechanism notifies the nsNPAPIPlugin if the plugin
  // crashes and is no longer usable. pluginDumpID/browserDumpID are
  // the IDs of respective minidumps that were written, or empty if no
  // minidump was written.
  void PluginCrashed(const nsAString& pluginDumpID,
                     const nsAString& browserDumpID);
  
  static bool RunPluginOOP(const nsPluginTag *aPluginTag);

  nsresult Shutdown();

  static nsresult RetainStream(NPStream *pstream, nsISupports **aRetainedPeer);

protected:
  NPPluginFuncs mPluginFuncs;
  PluginLibrary* mLibrary;
};

namespace mozilla {
namespace plugins {
namespace parent {

JS_STATIC_ASSERT(sizeof(NPIdentifier) == sizeof(jsid));

inline jsid
NPIdentifierToJSId(NPIdentifier id)
{
    jsid tmp;
    JSID_BITS(tmp) = (size_t)id;
    return tmp;
}

inline NPIdentifier
JSIdToNPIdentifier(jsid id)
{
    return (NPIdentifier)JSID_BITS(id);
}

inline bool
NPIdentifierIsString(NPIdentifier id)
{
    return JSID_IS_STRING(NPIdentifierToJSId(id));
}

inline JSString *
NPIdentifierToString(NPIdentifier id)
{
    return JSID_TO_STRING(NPIdentifierToJSId(id));
}

inline NPIdentifier
StringToNPIdentifier(JSContext *cx, JSString *str)
{
    return JSIdToNPIdentifier(INTERNED_STRING_TO_JSID(cx, str));
}

inline bool
NPIdentifierIsInt(NPIdentifier id)
{
    return JSID_IS_INT(NPIdentifierToJSId(id));
}

inline int
NPIdentifierToInt(NPIdentifier id)
{
    return JSID_TO_INT(NPIdentifierToJSId(id));
}

inline NPIdentifier
IntToNPIdentifier(int i)
{
    return JSIdToNPIdentifier(INT_TO_JSID(i));
}

JSContext* GetJSContext(NPP npp);

inline bool
NPStringIdentifierIsPermanent(NPP npp, NPIdentifier id)
{
  AutoSafeJSContext cx;
  return JS_StringHasBeenInterned(cx, NPIdentifierToString(id));
}

#define NPIdentifier_VOID (JSIdToNPIdentifier(JSID_VOID))

NPObject* NP_CALLBACK
_getwindowobject(NPP npp);

NPObject* NP_CALLBACK
_getpluginelement(NPP npp);

NPIdentifier NP_CALLBACK
_getstringidentifier(const NPUTF8* name);

void NP_CALLBACK
_getstringidentifiers(const NPUTF8** names, int32_t nameCount,
                      NPIdentifier *identifiers);

bool NP_CALLBACK
_identifierisstring(NPIdentifier identifiers);

NPIdentifier NP_CALLBACK
_getintidentifier(int32_t intid);

NPUTF8* NP_CALLBACK
_utf8fromidentifier(NPIdentifier identifier);

int32_t NP_CALLBACK
_intfromidentifier(NPIdentifier identifier);

NPObject* NP_CALLBACK
_createobject(NPP npp, NPClass* aClass);

NPObject* NP_CALLBACK
_retainobject(NPObject* npobj);

void NP_CALLBACK
_releaseobject(NPObject* npobj);

bool NP_CALLBACK
_invoke(NPP npp, NPObject* npobj, NPIdentifier method, const NPVariant *args,
        uint32_t argCount, NPVariant *result);

bool NP_CALLBACK
_invokeDefault(NPP npp, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result);

bool NP_CALLBACK
_evaluate(NPP npp, NPObject* npobj, NPString *script, NPVariant *result);

bool NP_CALLBACK
_getproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             NPVariant *result);

bool NP_CALLBACK
_setproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             const NPVariant *value);

bool NP_CALLBACK
_removeproperty(NPP npp, NPObject* npobj, NPIdentifier property);

bool NP_CALLBACK
_hasproperty(NPP npp, NPObject* npobj, NPIdentifier propertyName);

bool NP_CALLBACK
_hasmethod(NPP npp, NPObject* npobj, NPIdentifier methodName);

bool NP_CALLBACK
_enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier,
           uint32_t *count);

bool NP_CALLBACK
_construct(NPP npp, NPObject* npobj, const NPVariant *args,
           uint32_t argCount, NPVariant *result);

void NP_CALLBACK
_releasevariantvalue(NPVariant *variant);

void NP_CALLBACK
_setexception(NPObject* npobj, const NPUTF8 *message);

void NP_CALLBACK
_pushpopupsenabledstate(NPP npp, NPBool enabled);

void NP_CALLBACK
_poppopupsenabledstate(NPP npp);

typedef void(*PluginThreadCallback)(void *);

void NP_CALLBACK
_pluginthreadasynccall(NPP instance, PluginThreadCallback func,
                       void *userData);

NPError NP_CALLBACK
_getvalueforurl(NPP instance, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len);

NPError NP_CALLBACK
_setvalueforurl(NPP instance, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len);

NPError NP_CALLBACK
_getauthenticationinfo(NPP instance, const char *protocol, const char *host,
                       int32_t port, const char *scheme, const char *realm,
                       char **username, uint32_t *ulen, char **password,
                       uint32_t *plen);

typedef void(*PluginTimerFunc)(NPP npp, uint32_t timerID);

uint32_t NP_CALLBACK
_scheduletimer(NPP instance, uint32_t interval, NPBool repeat, PluginTimerFunc timerFunc);

void NP_CALLBACK
_unscheduletimer(NPP instance, uint32_t timerID);

NPError NP_CALLBACK
_popupcontextmenu(NPP instance, NPMenu* menu);

NPError NP_CALLBACK
_initasyncsurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface);

NPError NP_CALLBACK
_finalizeasyncsurface(NPP instance, NPAsyncSurface *surface);

void NP_CALLBACK
_setcurrentasyncsurface(NPP instance, NPAsyncSurface *surface, NPRect *changed);

NPBool NP_CALLBACK
_convertpoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);

NPError NP_CALLBACK
_requestread(NPStream *pstream, NPByteRange *rangeList);

NPError NP_CALLBACK
_geturlnotify(NPP npp, const char* relativeURL, const char* target,
              void* notifyData);

NPError NP_CALLBACK
_getvalue(NPP npp, NPNVariable variable, void *r_value);

NPError NP_CALLBACK
_setvalue(NPP npp, NPPVariable variable, void *r_value);

NPError NP_CALLBACK
_geturl(NPP npp, const char* relativeURL, const char* target);

NPError NP_CALLBACK
_posturlnotify(NPP npp, const char* relativeURL, const char *target,
               uint32_t len, const char *buf, NPBool file, void* notifyData);

NPError NP_CALLBACK
_posturl(NPP npp, const char* relativeURL, const char *target, uint32_t len,
            const char *buf, NPBool file);

NPError NP_CALLBACK
_newstream(NPP npp, NPMIMEType type, const char* window, NPStream** pstream);

int32_t NP_CALLBACK
_write(NPP npp, NPStream *pstream, int32_t len, void *buffer);

NPError NP_CALLBACK
_destroystream(NPP npp, NPStream *pstream, NPError reason);

void NP_CALLBACK
_status(NPP npp, const char *message);

void NP_CALLBACK
_memfree (void *ptr);

uint32_t NP_CALLBACK
_memflush(uint32_t size);

void NP_CALLBACK
_reloadplugins(NPBool reloadPages);

void NP_CALLBACK
_invalidaterect(NPP npp, NPRect *invalidRect);

void NP_CALLBACK
_invalidateregion(NPP npp, NPRegion invalidRegion);

void NP_CALLBACK
_forceredraw(NPP npp);

const char* NP_CALLBACK
_useragent(NPP npp);

void* NP_CALLBACK
_memalloc (uint32_t size);

// Deprecated entry points for the old Java plugin.
void* NP_CALLBACK /* OJI type: JRIEnv* */
_getJavaEnv();

void* NP_CALLBACK /* OJI type: jref */
_getJavaPeer(NPP npp);

void NP_CALLBACK
_urlredirectresponse(NPP instance, void* notifyData, NPBool allow);

} /* namespace parent */
} /* namespace plugins */
} /* namespace mozilla */

const char *
PeekException();

void
PopException();

void
OnPluginDestroy(NPP instance);

void
OnShutdown();

/**
 * within a lexical scope, locks and unlocks the mutex used to
 * serialize modifications to plugin async callback state.
 */
struct MOZ_STACK_CLASS AsyncCallbackAutoLock
{
  AsyncCallbackAutoLock();
  ~AsyncCallbackAutoLock();
};

class NPPStack
{
public:
  static NPP Peek()
  {
    return sCurrentNPP;
  }

protected:
  static NPP sCurrentNPP;
};

// XXXjst: The NPPAutoPusher stack is a bit redundant now that
// PluginDestructionGuard exists, and could thus be replaced by code
// that uses the PluginDestructionGuard list of plugins on the
// stack. But they're not identical, and to minimize code changes
// we're keeping both for the moment, and making NPPAutoPusher inherit
// the PluginDestructionGuard class to avoid having to keep two
// separate objects on the stack since we always want a
// PluginDestructionGuard where we use an NPPAutoPusher.

class MOZ_STACK_CLASS NPPAutoPusher : public NPPStack,
                                      protected PluginDestructionGuard
{
public:
  NPPAutoPusher(NPP npp)
    : PluginDestructionGuard(npp),
      mOldNPP(sCurrentNPP)
  {
    NS_ASSERTION(npp, "Uh, null npp passed to NPPAutoPusher!");

    sCurrentNPP = npp;
  }

  ~NPPAutoPusher()
  {
    sCurrentNPP = mOldNPP;
  }

private:
  NPP mOldNPP;
};

class NPPExceptionAutoHolder
{
public:
  NPPExceptionAutoHolder();
  ~NPPExceptionAutoHolder();

protected:
  char *mOldException;
};

#endif // nsNPAPIPlugin_h_
