/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNPAPIPlugin_h_
#define nsNPAPIPlugin_h_

#include "prlink.h"
#include "npfunctions.h"
#include "nsPluginHost.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/PluginLibrary.h"
#include "mozilla/RefCounted.h"

#if defined(XP_WIN)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)
#else
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (* _name)
#endif

typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks, NPPluginFuncs* fCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) ();

// nsNPAPIPlugin is held alive both by active nsPluginTag instances and
// by active nsNPAPIPluginInstance.
class nsNPAPIPlugin final
{
private:
  typedef mozilla::PluginLibrary PluginLibrary;

public:
  nsNPAPIPlugin();

  NS_INLINE_DECL_REFCOUNTING(nsNPAPIPlugin)

  // Constructs and initializes an nsNPAPIPlugin object. A nullptr file path
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

  nsresult Shutdown();

  static nsresult RetainStream(NPStream *pstream, nsISupports **aRetainedPeer);

private:
  ~nsNPAPIPlugin();

  NPPluginFuncs mPluginFuncs;
  PluginLibrary* mLibrary;
};

namespace mozilla {
namespace plugins {
namespace parent {

static_assert(sizeof(NPIdentifier) == sizeof(jsid),
              "NPIdentifier must be binary compatible with jsid.");

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
NPStringIdentifierIsPermanent(NPIdentifier id)
{
  AutoSafeJSContext cx;
  return JS_StringHasBeenPinned(cx, NPIdentifierToString(id));
}

#define NPIdentifier_VOID (JSIdToNPIdentifier(JSID_VOID))

NPObject*
_getwindowobject(NPP npp);

NPObject*
_getpluginelement(NPP npp);

NPIdentifier
_getstringidentifier(const NPUTF8* name);

void
_getstringidentifiers(const NPUTF8** names, int32_t nameCount,
                      NPIdentifier *identifiers);

bool
_identifierisstring(NPIdentifier identifiers);

NPIdentifier
_getintidentifier(int32_t intid);

NPUTF8*
_utf8fromidentifier(NPIdentifier identifier);

int32_t
_intfromidentifier(NPIdentifier identifier);

NPObject*
_createobject(NPP npp, NPClass* aClass);

NPObject*
_retainobject(NPObject* npobj);

void
_releaseobject(NPObject* npobj);

bool
_invoke(NPP npp, NPObject* npobj, NPIdentifier method, const NPVariant *args,
        uint32_t argCount, NPVariant *result);

bool
_invokeDefault(NPP npp, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result);

bool
_evaluate(NPP npp, NPObject* npobj, NPString *script, NPVariant *result);

bool
_getproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             NPVariant *result);

bool
_setproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             const NPVariant *value);

bool
_removeproperty(NPP npp, NPObject* npobj, NPIdentifier property);

bool
_hasproperty(NPP npp, NPObject* npobj, NPIdentifier propertyName);

bool
_hasmethod(NPP npp, NPObject* npobj, NPIdentifier methodName);

bool
_enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier,
           uint32_t *count);

bool
_construct(NPP npp, NPObject* npobj, const NPVariant *args,
           uint32_t argCount, NPVariant *result);

void
_releasevariantvalue(NPVariant *variant);

void
_setexception(NPObject* npobj, const NPUTF8 *message);

void
_pushpopupsenabledstate(NPP npp, NPBool enabled);

void
_poppopupsenabledstate(NPP npp);

NPError
_getvalueforurl(NPP instance, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len);

NPError
_setvalueforurl(NPP instance, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len);

typedef void(*PluginTimerFunc)(NPP npp, uint32_t timerID);

uint32_t
_scheduletimer(NPP instance, uint32_t interval, NPBool repeat, PluginTimerFunc timerFunc);

void
_unscheduletimer(NPP instance, uint32_t timerID);

NPError
_popupcontextmenu(NPP instance, NPMenu* menu);

NPBool
_convertpoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);

NPError
_requestread(NPStream *pstream, NPByteRange *rangeList);

NPError
_geturlnotify(NPP npp, const char* relativeURL, const char* target,
              void* notifyData);

NPError
_getvalue(NPP npp, NPNVariable variable, void *r_value);

NPError
_setvalue(NPP npp, NPPVariable variable, void *r_value);

NPError
_geturl(NPP npp, const char* relativeURL, const char* target);

NPError
_posturlnotify(NPP npp, const char* relativeURL, const char *target,
               uint32_t len, const char *buf, NPBool file, void* notifyData);

NPError
_posturl(NPP npp, const char* relativeURL, const char *target, uint32_t len,
            const char *buf, NPBool file);

void
_status(NPP npp, const char *message);

void
_memfree (void *ptr);

uint32_t
_memflush(uint32_t size);

void
_reloadplugins(NPBool reloadPages);

void
_invalidaterect(NPP npp, NPRect *invalidRect);

void
_invalidateregion(NPP npp, NPRegion invalidRegion);

void
_forceredraw(NPP npp);

const char*
_useragent(NPP npp);

void*
_memalloc (uint32_t size);

// Deprecated entry points for the old Java plugin.
void* /* OJI type: JRIEnv* */
_getJavaEnv();

void* /* OJI type: jref */
_getJavaPeer(NPP npp);

void
_urlredirectresponse(NPP instance, void* notifyData, NPBool allow);

NPError
_initasyncsurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface);

NPError
_finalizeasyncsurface(NPP instance, NPAsyncSurface *surface);

void
_setcurrentasyncsurface(NPP instance, NPAsyncSurface *surface, NPRect *changed);

} /* namespace parent */
} /* namespace plugins */
} /* namespace mozilla */

const char *
PeekException();

void
PopException();

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
  explicit NPPAutoPusher(NPP aNpp)
    : PluginDestructionGuard(aNpp),
      mOldNPP(sCurrentNPP)
  {
    NS_ASSERTION(aNpp, "Uh, null aNpp passed to NPPAutoPusher!");

    sCurrentNPP = aNpp;
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
