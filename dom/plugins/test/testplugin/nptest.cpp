/* ***** BEGIN LICENSE BLOCK *****
 *
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
 *   Josh Aas <josh@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "nptest.h"
#include "nptest_utils.h"
#include "nptest_platform.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/IntentionalCrash.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <ctime>

#ifdef XP_WIN
#include <process.h>
#include <float.h>
#include <windows.h>
#define getpid _getpid
#define strcasecmp _stricmp
#else
#include <unistd.h>
#include <pthread.h>
#endif

using namespace std;

#define PLUGIN_VERSION     "1.0.0.0"

extern const char *sPluginName;
extern const char *sPluginDescription;
static char sPluginVersion[] = PLUGIN_VERSION;

//
// Intentional crash
//

int gCrashCount = 0;

static void Crash()
{
  int *pi = nullptr;
  *pi = 55; // Crash dereferencing null pointer
  ++gCrashCount;
}

static void
IntentionalCrash()
{
  mozilla::NoteIntentionalCrash("plugin");
  Crash();
}

//
// static data
//

static NPNetscapeFuncs* sBrowserFuncs = nullptr;
static NPClass sNPClass;

void
asyncCallback(void* cookie);

//
// identifiers
//

typedef bool (* ScriptableFunction)
  (NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

static bool npnEvaluateTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool npnInvokeTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool npnInvokeDefaultTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setUndefinedValueTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool identifierToStringTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool timerTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool queryPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool lastReportedPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool hasWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getClipRegionRectCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getClipRegionRectEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool startWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool stopWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getLastMouseX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getLastMouseY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getPaintCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool resetPaintCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getWidthAtLastPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setInvalidateDuringPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setSlowPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getError(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool doInternalConsistencyCheck(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setColor(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool throwExceptionNextInvoke(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool convertPointX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool convertPointY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool streamTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool postFileToURLTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setPluginWantsAllStreams(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool crashPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool crashOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getJavaCodebase(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool checkObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool enableFPExceptions(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool asyncCallbackTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool checkGCRace(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool hangPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool stallPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getClipboardText(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool callOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool reinitWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool crashPluginInNestedLoop(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool triggerXError(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool destroySharedGfxStuff(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool propertyAndMethod(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getTopLevelWindowActivationState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getTopLevelWindowActivationEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getFocusState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getFocusEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getEventModel(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getReflector(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool isVisible(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getWindowPosition(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool constructObject(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setSitesWithData(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setSitesWithDataCapabilities(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getLastKeyText(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getNPNVdocumentOrigin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getMouseUpEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool queryContentsScaleFactor(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool queryCSSZoomFactorGetValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool queryCSSZoomFactorSetValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool echoString(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool startAudioPlayback(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool stopAudioPlayback(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getAudioMuted(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool nativeWidgetIsVisible(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getLastCompositionText(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getInvokeDefaultObject(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

static const NPUTF8* sPluginMethodIdentifierNames[] = {
  "npnEvaluateTest",
  "npnInvokeTest",
  "npnInvokeDefaultTest",
  "setUndefinedValueTest",
  "identifierToStringTest",
  "timerTest",
  "queryPrivateModeState",
  "lastReportedPrivateModeState",
  "hasWidget",
  "getEdge",
  "getClipRegionRectCount",
  "getClipRegionRectEdge",
  "startWatchingInstanceCount",
  "getInstanceCount",
  "stopWatchingInstanceCount",
  "getLastMouseX",
  "getLastMouseY",
  "getPaintCount",
  "resetPaintCount",
  "getWidthAtLastPaint",
  "setInvalidateDuringPaint",
  "setSlowPaint",
  "getError",
  "doInternalConsistencyCheck",
  "setColor",
  "throwExceptionNextInvoke",
  "convertPointX",
  "convertPointY",
  "streamTest",
  "postFileToURLTest",
  "setPluginWantsAllStreams",
  "crash",
  "crashOnDestroy",
  "getObjectValue",
  "getJavaCodebase",
  "checkObjectValue",
  "enableFPExceptions",
  "asyncCallbackTest",
  "checkGCRace",
  "hang",
  "stall",
  "getClipboardText",
  "callOnDestroy",
  "reinitWidget",
  "crashInNestedLoop",
  "triggerXError",
  "destroySharedGfxStuff",
  "propertyAndMethod",
  "getTopLevelWindowActivationState",
  "getTopLevelWindowActivationEventCount",
  "getFocusState",
  "getFocusEventCount",
  "getEventModel",
  "getReflector",
  "isVisible",
  "getWindowPosition",
  "constructObject",
  "setSitesWithData",
  "setSitesWithDataCapabilities",
  "getLastKeyText",
  "getNPNVdocumentOrigin",
  "getMouseUpEventCount",
  "queryContentsScaleFactor",
  "queryCSSZoomFactorSetValue",
  "queryCSSZoomFactorGetValue",
  "echoString",
  "startAudioPlayback",
  "stopAudioPlayback",
  "audioMuted",
  "nativeWidgetIsVisible",
  "getLastCompositionText",
  "getInvokeDefaultObject",
};
static NPIdentifier sPluginMethodIdentifiers[MOZ_ARRAY_LENGTH(sPluginMethodIdentifierNames)];
static const ScriptableFunction sPluginMethodFunctions[] = {
  npnEvaluateTest,
  npnInvokeTest,
  npnInvokeDefaultTest,
  setUndefinedValueTest,
  identifierToStringTest,
  timerTest,
  queryPrivateModeState,
  lastReportedPrivateModeState,
  hasWidget,
  getEdge,
  getClipRegionRectCount,
  getClipRegionRectEdge,
  startWatchingInstanceCount,
  getInstanceCount,
  stopWatchingInstanceCount,
  getLastMouseX,
  getLastMouseY,
  getPaintCount,
  resetPaintCount,
  getWidthAtLastPaint,
  setInvalidateDuringPaint,
  setSlowPaint,
  getError,
  doInternalConsistencyCheck,
  setColor,
  throwExceptionNextInvoke,
  convertPointX,
  convertPointY,
  streamTest,
  postFileToURLTest,
  setPluginWantsAllStreams,
  crashPlugin,
  crashOnDestroy,
  getObjectValue,
  getJavaCodebase,
  checkObjectValue,
  enableFPExceptions,
  asyncCallbackTest,
  checkGCRace,
  hangPlugin,
  stallPlugin,
  getClipboardText,
  callOnDestroy,
  reinitWidget,
  crashPluginInNestedLoop,
  triggerXError,
  destroySharedGfxStuff,
  propertyAndMethod,
  getTopLevelWindowActivationState,
  getTopLevelWindowActivationEventCount,
  getFocusState,
  getFocusEventCount,
  getEventModel,
  getReflector,
  isVisible,
  getWindowPosition,
  constructObject,
  setSitesWithData,
  setSitesWithDataCapabilities,
  getLastKeyText,
  getNPNVdocumentOrigin,
  getMouseUpEventCount,
  queryContentsScaleFactor,
  queryCSSZoomFactorGetValue,
  queryCSSZoomFactorSetValue,
  echoString,
  startAudioPlayback,
  stopAudioPlayback,
  getAudioMuted,
  nativeWidgetIsVisible,
  getLastCompositionText,
  getInvokeDefaultObject,
};

static_assert(MOZ_ARRAY_LENGTH(sPluginMethodIdentifierNames) ==
              MOZ_ARRAY_LENGTH(sPluginMethodFunctions),
              "Arrays should have the same size");

static const NPUTF8* sPluginPropertyIdentifierNames[] = {
  "propertyAndMethod"
};
static NPIdentifier sPluginPropertyIdentifiers[MOZ_ARRAY_LENGTH(sPluginPropertyIdentifierNames)];
static NPVariant sPluginPropertyValues[MOZ_ARRAY_LENGTH(sPluginPropertyIdentifierNames)];

struct URLNotifyData
{
  const char* cookie;
  NPObject* writeCallback;
  NPObject* notifyCallback;
  NPObject* redirectCallback;
  bool allowRedirects;
  uint32_t size;
  char* data;
};

static URLNotifyData kNotifyData = {
  "static-cookie",
  nullptr,
  nullptr,
  nullptr,
  false,
  0,
  nullptr
};

static const char* SUCCESS_STRING = "pass";

static bool sIdentifiersInitialized = false;

struct timerEvent {
  int32_t timerIdReceive;
  int32_t timerIdSchedule;
  uint32_t timerInterval;
  bool timerRepeat;
  int32_t timerIdUnschedule;
};
static timerEvent timerEvents[] = {
  {-1, 0, 200, false, -1},
  {0, 0, 400, false, -1},
  {0, 0, 200, true, -1},
  {0, 1, 400, true, -1},
  {0, -1, 0, false, 0},
  {1, -1, 0, false, -1},
  {1, -1, 0, false, 1},
};
static uint32_t currentTimerEventCount = 0;
static uint32_t totalTimerEvents = sizeof(timerEvents) / sizeof(timerEvent);

/**
 * Incremented for every startWatchingInstanceCount.
 */
static int32_t sCurrentInstanceCountWatchGeneration = 0;
/**
 * Tracks the number of instances created or destroyed since the last
 * startWatchingInstanceCount.
 */
static int32_t sInstanceCount = 0;
/**
 * True when we've had a startWatchingInstanceCount with no corresponding
 * stopWatchingInstanceCount.
 */
static bool sWatchingInstanceCount = false;

/**
 * A list representing sites for which the plugin has stored data. See
 * NPP_ClearSiteData and NPP_GetSitesWithData.
 */
struct siteData {
  string site;
  uint64_t flags;
  uint64_t age;
};
static list<siteData>* sSitesWithData;
static bool sClearByAgeSupported;

static void initializeIdentifiers()
{
  if (!sIdentifiersInitialized) {
    NPN_GetStringIdentifiers(sPluginMethodIdentifierNames,
        MOZ_ARRAY_LENGTH(sPluginMethodIdentifierNames), sPluginMethodIdentifiers);
    NPN_GetStringIdentifiers(sPluginPropertyIdentifierNames,
        MOZ_ARRAY_LENGTH(sPluginPropertyIdentifierNames), sPluginPropertyIdentifiers);

    sIdentifiersInitialized = true;

    // Check whether nullptr is handled in NPN_GetStringIdentifiers
    NPIdentifier IDList[2];
    static char const *const kIDNames[2] = { nullptr, "setCookie" };
    NPN_GetStringIdentifiers(const_cast<const NPUTF8**>(kIDNames), 2, IDList);
  }
}

static void clearIdentifiers()
{
  memset(sPluginMethodIdentifiers, 0,
      MOZ_ARRAY_LENGTH(sPluginMethodIdentifiers) * sizeof(NPIdentifier));
  memset(sPluginPropertyIdentifiers, 0,
      MOZ_ARRAY_LENGTH(sPluginPropertyIdentifiers) * sizeof(NPIdentifier));

  sIdentifiersInitialized = false;
}

static void addRange(InstanceData* instanceData, const char* range)
{
  /*
  increased rangestr size from 16 to 17, the 17byte is only for
  null terminated value, maybe for actual capacity it needs 16 bytes
  */
  char rangestr[17];
  memset(rangestr, 0, sizeof(rangestr));
  strncpy(rangestr, range, sizeof(rangestr) - sizeof(char));
  const char* str1 = strtok(rangestr, ",");
  const char* str2 = str1 ? strtok(nullptr, ",") : nullptr;
  if (str1 && str2) {
    TestRange* byterange = new TestRange;
    byterange->offset = atoi(str1);
    byterange->length = atoi(str2);
    byterange->waiting = true;
    byterange->next = instanceData->testrange;
    instanceData->testrange = byterange;
  }
}

static void sendBufferToFrame(NPP instance)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  string outbuf;
  if (!instanceData->npnNewStream) outbuf = "data:text/html,";
  const char* buf = reinterpret_cast<char *>(instanceData->streamBuf);
  int32_t bufsize = instanceData->streamBufSize;
  if (instanceData->streamMode == NP_ASFILE ||
      instanceData->streamMode == NP_ASFILEONLY) {
    buf = reinterpret_cast<char *>(instanceData->fileBuf);
    bufsize = instanceData->fileBufSize;
  }
  if (instanceData->err.str().length() > 0) {
    outbuf.append(instanceData->err.str());
  }
  else if (bufsize > 0) {
    outbuf.append(buf);
  }
  else {
    outbuf.append("Error: no data in buffer");
  }

  if (instanceData->npnNewStream &&
      instanceData->err.str().length() == 0) {
    char typeHTML[] = "text/html";
    NPStream* stream;
    printf("calling NPN_NewStream...");
    NPError err = NPN_NewStream(instance, typeHTML,
        instanceData->frame.c_str(), &stream);
    printf("return value %d\n", err);
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_NewStream returned " << err;
      return;
    }

    int32_t bytesToWrite = outbuf.length();
    int32_t bytesWritten = 0;
    while ((bytesToWrite - bytesWritten) > 0) {
      int32_t numBytes = (bytesToWrite - bytesWritten) <
          instanceData->streamChunkSize ?
          bytesToWrite - bytesWritten : instanceData->streamChunkSize;
      int32_t written = NPN_Write(instance, stream,
          numBytes, (void*)(outbuf.c_str() + bytesWritten));
      if (written <= 0) {
        instanceData->err << "NPN_Write returned " << written;
        break;
      }
      bytesWritten += numBytes;
      printf("%d bytes written, total %d\n", written, bytesWritten);
    }
    err = NPN_DestroyStream(instance, stream, NPRES_DONE);
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_DestroyStream returned " << err;
    }
  }
  else {
    // Convert CRLF to LF, and escape most other non-alphanumeric chars.
    for (size_t i = 0; i < outbuf.length(); i++) {
      if (outbuf[i] == '\n') {
        outbuf.replace(i, 1, "%0a");
        i += 2;
      }
      else if (outbuf[i] == '\r') {
        outbuf.replace(i, 1, "");
        i -= 1;
      }
      else {
        int ascii = outbuf[i];
        if (!((ascii >= ',' && ascii <= ';') ||
              (ascii >= 'A' && ascii <= 'Z') ||
              (ascii >= 'a' && ascii <= 'z'))) {
          char hex[10];
          sprintf(hex, "%%%x", ascii);
          outbuf.replace(i, 1, hex);
          i += 2;
        }
      }
    }

    NPError err = NPN_GetURL(instance, outbuf.c_str(),
                             instanceData->frame.c_str());
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_GetURL returned " << err;
    }
  }
}

static void XPSleep(unsigned int seconds)
{
#ifdef XP_WIN
  Sleep(1000 * seconds);
#else
  sleep(seconds);
#endif
}

TestFunction
getFuncFromString(const char* funcname)
{
  FunctionTable funcTable[] =
    {
      { FUNCTION_NPP_NEWSTREAM, "npp_newstream" },
      { FUNCTION_NPP_WRITEREADY, "npp_writeready" },
      { FUNCTION_NPP_WRITE, "npp_write" },
      { FUNCTION_NPP_DESTROYSTREAM, "npp_destroystream" },
      { FUNCTION_NPP_WRITE_RPC, "npp_write_rpc" },
      { FUNCTION_NONE, nullptr }
    };
  int32_t i = 0;
  while(funcTable[i].funcName) {
    if (!strcmp(funcname, funcTable[i].funcName)) return funcTable[i].funcId;
    i++;
  }
  return FUNCTION_NONE;
}

static void
DuplicateNPVariant(NPVariant& aDest, const NPVariant& aSrc)
{
  if (NPVARIANT_IS_STRING(aSrc)) {
    NPString src = NPVARIANT_TO_STRING(aSrc);
    char* buf = new char[src.UTF8Length];
    strncpy(buf, src.UTF8Characters, src.UTF8Length);
    STRINGN_TO_NPVARIANT(buf, src.UTF8Length, aDest);
  }
  else if (NPVARIANT_IS_OBJECT(aSrc)) {
    NPObject* obj =
      NPN_RetainObject(NPVARIANT_TO_OBJECT(aSrc));
    OBJECT_TO_NPVARIANT(obj, aDest);
  }
  else {
    aDest = aSrc;
  }
}

static bool bug813906(NPP npp, const char* const function, const char* const url, const char* const frame)
{
  NPObject *windowObj = nullptr;
  NPError err = NPN_GetValue(npp, NPNVWindowNPObject, &windowObj);
  if (err != NPERR_NO_ERROR) {
    return false;
  }

  NPVariant result;
  bool res = NPN_Invoke(npp, windowObj, NPN_GetStringIdentifier(function), nullptr, 0, &result);
  NPN_ReleaseObject(windowObj);
  if (!res) {
    return false;
  }

  NPN_ReleaseVariantValue(&result);

  err = NPN_GetURL(npp, url, frame);
  if (err != NPERR_NO_ERROR) {
    err = NPN_GetURL(npp, "about:blank", frame);
    return false;
  }

  return true;
}

void
drawAsyncBitmapColor(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;

  uint32_t *pixelData = (uint32_t*)instanceData->backBuffer->bitmap.data;

  uint32_t rgba = instanceData->scriptableObject->drawColor;

  unsigned char subpixels[4];
  memcpy(subpixels, &rgba, sizeof(subpixels));

  subpixels[0] = uint8_t(float(subpixels[3] * subpixels[0]) / 0xFF);
  subpixels[1] = uint8_t(float(subpixels[3] * subpixels[1]) / 0xFF);
  subpixels[2] = uint8_t(float(subpixels[3] * subpixels[2]) / 0xFF);
  uint32_t premultiplied;
  memcpy(&premultiplied, subpixels, sizeof(premultiplied));

  for (uint32_t* lastPixel = pixelData + instanceData->backBuffer->size.width * instanceData->backBuffer->size.height;
       pixelData < lastPixel;
       ++pixelData)
  {
    *pixelData = premultiplied;
  }

  NPN_SetCurrentAsyncSurface(npp, instanceData->backBuffer, NULL);
  NPAsyncSurface *oldFront = instanceData->frontBuffer;
  instanceData->frontBuffer = instanceData->backBuffer;
  instanceData->backBuffer = oldFront;
}

//
// function signatures
//

NPObject* scriptableAllocate(NPP npp, NPClass* aClass);
void scriptableDeallocate(NPObject* npobj);
void scriptableInvalidate(NPObject* npobj);
bool scriptableHasMethod(NPObject* npobj, NPIdentifier name);
bool scriptableInvoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
bool scriptableHasProperty(NPObject* npobj, NPIdentifier name);
bool scriptableGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result);
bool scriptableSetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value);
bool scriptableRemoveProperty(NPObject* npobj, NPIdentifier name);
bool scriptableEnumerate(NPObject* npobj, NPIdentifier** identifier, uint32_t* count);
bool scriptableConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

//
// npapi plugin functions
//

#ifdef XP_UNIX
NP_EXPORT(char*)
NP_GetPluginVersion()
{
  return sPluginVersion;
}
#endif

extern const char *sMimeDescription;

#if defined(XP_UNIX)
NP_EXPORT(const char*) NP_GetMIMEDescription()
#elif defined(XP_WIN)
const char* NP_GetMIMEDescription()
#endif
{
  return sMimeDescription;
}

#ifdef XP_UNIX
NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {
  switch (aVariable) {
    case NPPVpluginNameString:
      *((const char**)aValue) = sPluginName;
      break;
    case NPPVpluginDescriptionString:
      *((const char**)aValue) = sPluginDescription;
      break;
    default:
      return NPERR_INVALID_PARAM;
  }
  return NPERR_NO_ERROR;
}
#endif

static bool fillPluginFunctionTable(NPPluginFuncs* pFuncs)
{
  // Check the size of the provided structure based on the offset of the
  // last member we need.
  if (pFuncs->size < (offsetof(NPPluginFuncs, getsiteswithdata) + sizeof(void*)))
    return false;

  pFuncs->newp = NPP_New;
  pFuncs->destroy = NPP_Destroy;
  pFuncs->setwindow = NPP_SetWindow;
  pFuncs->newstream = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile = NPP_StreamAsFile;
  pFuncs->writeready = NPP_WriteReady;
  pFuncs->write = NPP_Write;
  pFuncs->print = NPP_Print;
  pFuncs->event = NPP_HandleEvent;
  pFuncs->urlnotify = NPP_URLNotify;
  pFuncs->getvalue = NPP_GetValue;
  pFuncs->setvalue = NPP_SetValue;
  pFuncs->urlredirectnotify = NPP_URLRedirectNotify;
  pFuncs->clearsitedata = NPP_ClearSiteData;
  pFuncs->getsiteswithdata = NPP_GetSitesWithData;

  return true;
}

#if defined(XP_MACOSX)
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs)
#elif defined(XP_WIN)
NPError OSCALL NP_Initialize(NPNetscapeFuncs* bFuncs)
#elif defined(XP_UNIX)
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
#endif
{
  sBrowserFuncs = bFuncs;

  initializeIdentifiers();

  for (unsigned int i = 0; i < MOZ_ARRAY_LENGTH(sPluginPropertyValues); i++) {
    VOID_TO_NPVARIANT(sPluginPropertyValues[i]);
  }

  memset(&sNPClass, 0, sizeof(NPClass));
  sNPClass.structVersion =  NP_CLASS_STRUCT_VERSION;
  sNPClass.allocate =       (NPAllocateFunctionPtr)scriptableAllocate;
  sNPClass.deallocate =     (NPDeallocateFunctionPtr)scriptableDeallocate;
  sNPClass.invalidate =     (NPInvalidateFunctionPtr)scriptableInvalidate;
  sNPClass.hasMethod =      (NPHasMethodFunctionPtr)scriptableHasMethod;
  sNPClass.invoke =         (NPInvokeFunctionPtr)scriptableInvoke;
  sNPClass.invokeDefault =  nullptr;
  sNPClass.hasProperty =    (NPHasPropertyFunctionPtr)scriptableHasProperty;
  sNPClass.getProperty =    (NPGetPropertyFunctionPtr)scriptableGetProperty;
  sNPClass.setProperty =    (NPSetPropertyFunctionPtr)scriptableSetProperty;
  sNPClass.removeProperty = (NPRemovePropertyFunctionPtr)scriptableRemoveProperty;
  sNPClass.enumerate =      (NPEnumerationFunctionPtr)scriptableEnumerate;
  sNPClass.construct =      (NPConstructFunctionPtr)scriptableConstruct;

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  if (!fillPluginFunctionTable(pFuncs)) {
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }
#endif

  return NPERR_NO_ERROR;
}

#if defined(XP_MACOSX)
NP_EXPORT(NPError) NP_GetEntryPoints(NPPluginFuncs* pFuncs)
#elif defined(XP_WIN)
NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs)
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
{
  if (!fillPluginFunctionTable(pFuncs)) {
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }

  return NPERR_NO_ERROR;
}
#endif

#if defined(XP_UNIX)
NP_EXPORT(NPError) NP_Shutdown()
#elif defined(XP_WIN)
NPError OSCALL NP_Shutdown()
#endif
{
  clearIdentifiers();

  for (unsigned int i = 0; i < MOZ_ARRAY_LENGTH(sPluginPropertyValues); i++) {
    NPN_ReleaseVariantValue(&sPluginPropertyValues[i]);
  }

  return NPERR_NO_ERROR;
}

NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
  // Make sure our pdata field is nullptr at this point. If it isn't, that
  // probably means the browser gave us uninitialized memory.
  if (instance->pdata) {
    printf("NPP_New called with non-NULL NPP->pdata pointer!\n");
    return NPERR_GENERIC_ERROR;
  }

  // Make sure we can render this plugin
  NPBool browserSupportsWindowless = false;
  NPN_GetValue(instance, NPNVSupportsWindowless, &browserSupportsWindowless);
  if (!browserSupportsWindowless && !pluginSupportsWindowMode()) {
    printf("Windowless mode not supported by the browser, windowed mode not supported by the plugin!\n");
    return NPERR_GENERIC_ERROR;
  }

  // set up our our instance data
  InstanceData* instanceData = new InstanceData;
  instanceData->npp = instance;
  instanceData->streamMode = NP_ASFILEONLY;
  instanceData->testFunction = FUNCTION_NONE;
  instanceData->functionToFail = FUNCTION_NONE;
  instanceData->failureCode = 0;
  instanceData->callOnDestroy = nullptr;
  instanceData->streamChunkSize = 1024;
  instanceData->streamBuf = nullptr;
  instanceData->streamBufSize = 0;
  instanceData->fileBuf = nullptr;
  instanceData->fileBufSize = 0;
  instanceData->throwOnNextInvoke = false;
  instanceData->runScriptOnPaint = false;
  instanceData->dontTouchElement = false;
  instanceData->testrange = nullptr;
  instanceData->hasWidget = false;
  instanceData->npnNewStream = false;
  instanceData->invalidateDuringPaint = false;
  instanceData->slowPaint = false;
  instanceData->playingAudio = false;
  instanceData->audioMuted = false;
  instanceData->writeCount = 0;
  instanceData->writeReadyCount = 0;
  memset(&instanceData->window, 0, sizeof(instanceData->window));
  instanceData->crashOnDestroy = false;
  instanceData->cleanupWidget = true; // only used by nptest_gtk
  instanceData->topLevelWindowActivationState = ACTIVATION_STATE_UNKNOWN;
  instanceData->topLevelWindowActivationEventCount = 0;
  instanceData->focusState = ACTIVATION_STATE_UNKNOWN;
  instanceData->focusEventCount = 0;
  instanceData->eventModel = 0;
  instanceData->closeStream = false;
  instanceData->wantsAllStreams = false;
  instanceData->mouseUpEventCount = 0;
  instanceData->bugMode = -1;
  instanceData->asyncDrawing = AD_NONE;
  instanceData->frontBuffer = nullptr;
  instanceData->backBuffer = nullptr;
  instanceData->placeholderWnd = nullptr;
  instanceData->cssZoomFactor = 1.0;
  instance->pdata = instanceData;

  TestNPObject* scriptableObject = (TestNPObject*)NPN_CreateObject(instance, &sNPClass);
  if (!scriptableObject) {
    printf("NPN_CreateObject failed to create an object, can't create a plugin instance\n");
    delete instanceData;
    return NPERR_GENERIC_ERROR;
  }
  scriptableObject->npp = instance;
  scriptableObject->drawMode = DM_DEFAULT;
  scriptableObject->drawColor = 0;
  instanceData->scriptableObject = scriptableObject;

  instanceData->instanceCountWatchGeneration = sCurrentInstanceCountWatchGeneration;

  AsyncDrawing requestAsyncDrawing = AD_NONE;

  bool requestWindow = false;
  bool alreadyHasSalign = false;
  // handle extra params
  for (int i = 0; i < argc; i++) {
    if (strcmp(argn[i], "drawmode") == 0) {
      if (strcmp(argv[i], "solid") == 0)
        scriptableObject->drawMode = DM_SOLID_COLOR;
    }
    else if (strcmp(argn[i], "color") == 0) {
      scriptableObject->drawColor = parseHexColor(argv[i], strlen(argv[i]));
    }
    else if (strcmp(argn[i], "wmode") == 0) {
      if (strcmp(argv[i], "window") == 0) {
        requestWindow = true;
      }
    }
    else if (strcmp(argn[i], "asyncmodel") == 0) {
      if (strcmp(argv[i], "bitmap") == 0) {
        requestAsyncDrawing = AD_BITMAP;
      } else if (strcmp(argv[i], "dxgi") == 0) {
        requestAsyncDrawing = AD_DXGI;
      }
    }
    if (strcmp(argn[i], "streammode") == 0) {
      if (strcmp(argv[i], "normal") == 0) {
        instanceData->streamMode = NP_NORMAL;
      }
      else if ((strcmp(argv[i], "asfile") == 0) &&
                strlen(argv[i]) == strlen("asfile")) {
        instanceData->streamMode = NP_ASFILE;
      }
      else if (strcmp(argv[i], "asfileonly") == 0) {
        instanceData->streamMode = NP_ASFILEONLY;
      }
      else if (strcmp(argv[i], "seek") == 0) {
        instanceData->streamMode = NP_SEEK;
      }
    }
    if (strcmp(argn[i], "streamchunksize") == 0) {
      instanceData->streamChunkSize = atoi(argv[i]);
    }
    if (strcmp(argn[i], "failurecode") == 0) {
      instanceData->failureCode = atoi(argv[i]);
    }
    if (strcmp(argn[i], "functiontofail") == 0) {
      instanceData->functionToFail = getFuncFromString(argv[i]);
    }
    if (strcmp(argn[i], "geturl") == 0) {
      instanceData->testUrl = argv[i];
      instanceData->testFunction = FUNCTION_NPP_GETURL;
    }
    if (strcmp(argn[i], "posturl") == 0) {
      instanceData->testUrl = argv[i];
      instanceData->testFunction = FUNCTION_NPP_POSTURL;
    }
    if (strcmp(argn[i], "geturlnotify") == 0) {
      instanceData->testUrl = argv[i];
      instanceData->testFunction = FUNCTION_NPP_GETURLNOTIFY;
    }
    if (strcmp(argn[i], "postmode") == 0) {
      if (strcmp(argv[i], "frame") == 0) {
        instanceData->postMode = POSTMODE_FRAME;
      }
      else if (strcmp(argv[i], "stream") == 0) {
        instanceData->postMode = POSTMODE_STREAM;
      }
    }
    if (strcmp(argn[i], "frame") == 0) {
      instanceData->frame = argv[i];
    }
    if (strcmp(argn[i], "range") == 0) {
      string range = argv[i];
      size_t semicolon = range.find(';');
      while (semicolon != string::npos) {
        addRange(instanceData, range.substr(0, semicolon).c_str());
        if (semicolon == range.length()) {
          range = "";
          break;
        }
        range = range.substr(semicolon + 1);
        semicolon = range.find(';');
      }
      if (range.length()) addRange(instanceData, range.c_str());
    }
    if (strcmp(argn[i], "newstream") == 0 &&
        strcmp(argv[i], "true") == 0) {
      instanceData->npnNewStream = true;
    }
    if (strcmp(argn[i], "newcrash") == 0) {
      IntentionalCrash();
    }
    if (strcmp(argn[i], "paintscript") == 0) {
      instanceData->runScriptOnPaint = true;
    }

    if (strcmp(argn[i], "donttouchelement") == 0) {
      instanceData->dontTouchElement = true;
    }
    // "cleanupwidget" is only used with nptest_gtk, defaulting to true.  It
    // indicates whether the plugin should destroy its window in response to
    // NPP_Destroy (or let the platform destroy the widget when the parent
    // window gets destroyed).
    if (strcmp(argn[i], "cleanupwidget") == 0 &&
        strcmp(argv[i], "false") == 0) {
      instanceData->cleanupWidget = false;
    }
    if (!strcmp(argn[i], "closestream")) {
      instanceData->closeStream = true;
    }
    if (strcmp(argn[i], "bugmode") == 0) {
      instanceData->bugMode = atoi(argv[i]);
    }
    // Try to emulate java's codebase handling: Use the last seen codebase
    // value, regardless of whether it is in attributes or params.
    if (strcasecmp(argn[i], "codebase") == 0) {
      instanceData->javaCodebase = argv[i];
    }

    // Bug 1307694 - There are two flash parameters that are order dependent for
    // scaling/sizing the plugin. If they ever change from what is expected, it
    // breaks flash on the web. In a test, if the scale tag ever happens
    // with an salign before it, fail the plugin creation.
    if (strcmp(argn[i], "scale") == 0) {
      if (alreadyHasSalign) {
        // If salign came before this parameter, error out now.
        return NPERR_GENERIC_ERROR;
      }
    }
    if (strcmp(argn[i], "salign") == 0) {
      alreadyHasSalign = true;
    }

    // We don't support NP_FULL any more, but name="plugin" is an indication
    // that we're a full-page plugin. We use default seek parameters for
    // test_fullpage.html
    if (strcmp(argn[i], "name") == 0 && strcmp(argv[i], "plugin") == 0) {
      instanceData->streamMode = NP_SEEK;
      instanceData->frame = "testframe";
      addRange(instanceData, "100,100");
    }
  }

  if (!browserSupportsWindowless || !pluginSupportsWindowlessMode()) {
    requestWindow = true;
  } else if (!pluginSupportsWindowMode()) {
    requestWindow = false;
  }
  if (requestWindow) {
    instanceData->hasWidget = true;
  } else {
    // NPPVpluginWindowBool should default to true, so we may as well
    // test that by not setting it in the window case
    NPN_SetValue(instance, NPPVpluginWindowBool, (void*)false);
  }

  if (scriptableObject->drawMode == DM_SOLID_COLOR &&
      (scriptableObject->drawColor & 0xFF000000) != 0xFF000000) {
    NPN_SetValue(instance, NPPVpluginTransparentBool, (void*)true);
  }

  if (requestAsyncDrawing == AD_BITMAP) {
    NPBool supportsAsyncBitmap = false;
    if ((NPN_GetValue(instance, NPNVsupportsAsyncBitmapSurfaceBool, &supportsAsyncBitmap) == NPERR_NO_ERROR) &&
        supportsAsyncBitmap) {
      if (NPN_SetValue(instance, NPPVpluginDrawingModel, (void*)NPDrawingModelAsyncBitmapSurface) == NPERR_NO_ERROR) {
        instanceData->asyncDrawing = AD_BITMAP;
      }
    }
  }
#ifdef XP_WIN
  else if (requestAsyncDrawing == AD_DXGI) {
    NPBool supportsAsyncDXGI = false;
    if ((NPN_GetValue(instance, NPNVsupportsAsyncWindowsDXGISurfaceBool, &supportsAsyncDXGI) == NPERR_NO_ERROR) &&
        supportsAsyncDXGI) {
      if (NPN_SetValue(instance, NPPVpluginDrawingModel, (void*)NPDrawingModelAsyncWindowsDXGISurface) == NPERR_NO_ERROR) {
        instanceData->asyncDrawing = AD_DXGI;
      }
    }
  }
#endif

  // If we can't get the right drawing mode, we fail, otherwise our tests might
  // appear to be passing when they shouldn't. Real plugins should not do this.
  if (instanceData->asyncDrawing != requestAsyncDrawing) {
    return NPERR_GENERIC_ERROR;
  }

  instanceData->lastReportedPrivateModeState = false;
  instanceData->lastMouseX = instanceData->lastMouseY = -1;
  instanceData->widthAtLastPaint = -1;
  instanceData->paintCount = 0;

  // do platform-specific initialization
  NPError err = pluginInstanceInit(instanceData);
  if (err != NPERR_NO_ERROR) {
    NPN_ReleaseObject(scriptableObject);
    delete instanceData;
    return err;
  }

  NPVariant variantTrue;
  BOOLEAN_TO_NPVARIANT(true, variantTrue);
  NPObject* o = nullptr;

  // Set a property on NPNVPluginElementNPObject, unless the consumer explicitly
  // opted out of this behavior.
  if (!instanceData->dontTouchElement) {
    err = NPN_GetValue(instance, NPNVPluginElementNPObject, &o);
    if (err == NPERR_NO_ERROR) {
      NPN_SetProperty(instance, o,
                      NPN_GetStringIdentifier("pluginFoundElement"), &variantTrue);
      NPN_ReleaseObject(o);
      o = nullptr;
    }
  }

  // Set a property on NPNVWindowNPObject
  err = NPN_GetValue(instance, NPNVWindowNPObject, &o);
  if (err == NPERR_NO_ERROR) {
    NPN_SetProperty(instance, o,
                    NPN_GetStringIdentifier("pluginFoundWindow"), &variantTrue);
    NPN_ReleaseObject(o);
    o = nullptr;
  }

  ++sInstanceCount;

  if (instanceData->testFunction == FUNCTION_NPP_GETURL) {
    NPError err = NPN_GetURL(instance, instanceData->testUrl.c_str(), nullptr);
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_GetURL returned " << err;
    }
  }
  else if (instanceData->testFunction == FUNCTION_NPP_GETURLNOTIFY) {
    NPError err = NPN_GetURLNotify(instance, instanceData->testUrl.c_str(),
                                   nullptr, static_cast<void*>(&kNotifyData));
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_GetURLNotify returned " << err;
    }
  }

  if ((instanceData->bugMode == 813906) && instanceData->frame.length()) {
    bug813906(instance, "f", "browser.xul", instanceData->frame.c_str());
  }

  return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->crashOnDestroy)
    IntentionalCrash();

  if (instanceData->callOnDestroy) {
    NPVariant result;
    NPN_InvokeDefault(instance, instanceData->callOnDestroy, nullptr, 0, &result);
    NPN_ReleaseVariantValue(&result);
    NPN_ReleaseObject(instanceData->callOnDestroy);
  }

  if (instanceData->streamBuf) {
    free(instanceData->streamBuf);
  }
  if (instanceData->fileBuf) {
    free(instanceData->fileBuf);
  }

  TestRange* currentrange = instanceData->testrange;
  TestRange* nextrange;
  while (currentrange != nullptr) {
    nextrange = reinterpret_cast<TestRange*>(currentrange->next);
    delete currentrange;
    currentrange = nextrange;
  }

  if (instanceData->frontBuffer) {
    NPN_SetCurrentAsyncSurface(instance, nullptr, nullptr);
    NPN_FinalizeAsyncSurface(instance, instanceData->frontBuffer);
    NPN_MemFree(instanceData->frontBuffer);
  }
  if (instanceData->backBuffer) {
    NPN_FinalizeAsyncSurface(instance, instanceData->backBuffer);
    NPN_MemFree(instanceData->backBuffer);
  }

  pluginInstanceShutdown(instanceData);
  NPN_ReleaseObject(instanceData->scriptableObject);

  if (sCurrentInstanceCountWatchGeneration == instanceData->instanceCountWatchGeneration) {
    --sInstanceCount;
  }
  delete instanceData;

  return NPERR_NO_ERROR;
}

NPError
NPP_SetWindow(NPP instance, NPWindow* window)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->scriptableObject->drawMode == DM_DEFAULT &&
      (instanceData->window.width != window->width ||
       instanceData->window.height != window->height)) {
    NPRect r;
    r.left = r.top = 0;
    r.right = window->width;
    r.bottom = window->height;
    NPN_InvalidateRect(instance, &r);
  }

  void* oldWindow = instanceData->window.window;
  pluginDoSetWindow(instanceData, window);
  if (instanceData->hasWidget && oldWindow != instanceData->window.window) {
    pluginWidgetInit(instanceData, oldWindow);
  }


  if (instanceData->asyncDrawing != AD_NONE) {
    if (instanceData->frontBuffer &&
        instanceData->frontBuffer->size.width >= 0 &&
        (uint32_t)instanceData->frontBuffer->size.width == window->width &&
        instanceData ->frontBuffer->size.height >= 0 &&
        (uint32_t)instanceData->frontBuffer->size.height == window->height)
    {
      return NPERR_NO_ERROR;
    }
    if (instanceData->frontBuffer) {
      NPN_FinalizeAsyncSurface(instance, instanceData->frontBuffer);
      NPN_MemFree(instanceData->frontBuffer);
    }
    if (instanceData->backBuffer) {
      NPN_FinalizeAsyncSurface(instance, instanceData->backBuffer);
      NPN_MemFree(instanceData->backBuffer);
    }
    instanceData->frontBuffer = (NPAsyncSurface*)NPN_MemAlloc(sizeof(NPAsyncSurface));
    instanceData->backBuffer = (NPAsyncSurface*)NPN_MemAlloc(sizeof(NPAsyncSurface));

    NPSize size;
    size.width = window->width;
    size.height = window->height;

    memcpy(instanceData->backBuffer, instanceData->frontBuffer, sizeof(NPAsyncSurface));

    NPN_InitAsyncSurface(instance, &size, NPImageFormatBGRA32, nullptr, instanceData->frontBuffer);
    NPN_InitAsyncSurface(instance, &size, NPImageFormatBGRA32, nullptr, instanceData->backBuffer);

#if defined(XP_WIN)
    if (instanceData->asyncDrawing == AD_DXGI) {
      if (!setupDxgiSurfaces(instance, instanceData)) {
        return NPERR_GENERIC_ERROR;
      }
    }
#endif
  }

  if (instanceData->asyncDrawing == AD_BITMAP) {
    drawAsyncBitmapColor(instanceData);
  }
#if defined(XP_WIN)
  else if (instanceData->asyncDrawing == AD_DXGI) {
    drawDxgiBitmapColor(instanceData);
  }
#endif

  return NPERR_NO_ERROR;
}

NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM &&
      instanceData->failureCode) {
    instanceData->err << SUCCESS_STRING;
    if (instanceData->frame.length() > 0) {
      sendBufferToFrame(instance);
    }
    return instanceData->failureCode;
  }

  if (stream->notifyData &&
      static_cast<URLNotifyData*>(stream->notifyData) != &kNotifyData) {
    // stream from streamTest
    *stype = NP_NORMAL;
  }
  else {
    *stype = instanceData->streamMode;

    if (instanceData->streamBufSize) {
      free(instanceData->streamBuf);
      instanceData->streamBufSize = 0;
      if (instanceData->testFunction == FUNCTION_NPP_POSTURL &&
          instanceData->postMode == POSTMODE_STREAM) {
        instanceData->testFunction = FUNCTION_NPP_GETURL;
      }
      else {
        // We already got a stream and didn't ask for another one.
        instanceData->err << "Received unexpected multiple NPP_NewStream";
      }
    }
  }
  return NPERR_NO_ERROR;
}

NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM) {
    instanceData->err << "NPP_DestroyStream called";
  }

  if (instanceData->functionToFail == FUNCTION_NPP_WRITE) {
    if (instanceData->writeCount == 1)
      instanceData->err << SUCCESS_STRING;
    else
      instanceData->err << "NPP_Write called after returning -1";
  }

  if (instanceData->functionToFail == FUNCTION_NPP_DESTROYSTREAM &&
      instanceData->failureCode) {
    instanceData->err << SUCCESS_STRING;
    if (instanceData->frame.length() > 0) {
      sendBufferToFrame(instance);
    }
    return instanceData->failureCode;
  }

  URLNotifyData* nd = static_cast<URLNotifyData*>(stream->notifyData);
  if (nd && nd != &kNotifyData) {
    return NPERR_NO_ERROR;
  }

  if (instanceData->streamMode == NP_ASFILE &&
      instanceData->functionToFail == FUNCTION_NONE) {
    if (!instanceData->streamBuf) {
      instanceData->err <<
        "Error: no data written with NPP_Write";
      return NPERR_GENERIC_ERROR;
    }

    if (!instanceData->fileBuf) {
      instanceData->err <<
        "Error: no data written with NPP_StreamAsFile";
      return NPERR_GENERIC_ERROR;
    }

    if (strcmp(reinterpret_cast<char *>(instanceData->fileBuf),
               reinterpret_cast<char *>(instanceData->streamBuf))) {
      instanceData->err <<
        "Error: data passed to NPP_Write and NPP_StreamAsFile differed";
    }
  }
  if (instanceData->frame.length() > 0 &&
      instanceData->testFunction != FUNCTION_NPP_GETURLNOTIFY &&
      instanceData->testFunction != FUNCTION_NPP_POSTURL) {
    sendBufferToFrame(instance);
  }
  if (instanceData->testFunction == FUNCTION_NPP_POSTURL) {
    NPError err = NPN_PostURL(instance, instanceData->testUrl.c_str(),
      instanceData->postMode == POSTMODE_FRAME ? instanceData->frame.c_str() : nullptr,
      instanceData->streamBufSize,
      reinterpret_cast<char *>(instanceData->streamBuf), false);
    if (err != NPERR_NO_ERROR)
      instanceData->err << "Error: NPN_PostURL returned error value " << err;
  }
  return NPERR_NO_ERROR;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  instanceData->writeReadyCount++;
  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM) {
    instanceData->err << "NPP_WriteReady called";
  }

  // temporarily disabled per bug 519870
  //if (instanceData->writeReadyCount == 1) {
  //  return 0;
  //}

  return instanceData->streamChunkSize;
}

int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  instanceData->writeCount++;

  // temporarily disabled per bug 519870
  //if (instanceData->writeReadyCount == 1) {
  //  instanceData->err << "NPP_Write called even though NPP_WriteReady " <<
  //      "returned 0";
  //}

  if (instanceData->functionToFail == FUNCTION_NPP_WRITE_RPC) {
    // Make an RPC call and pretend to consume the data
    NPObject* windowObject = nullptr;
    NPN_GetValue(instance, NPNVWindowNPObject, &windowObject);
    if (windowObject)
      NPN_ReleaseObject(windowObject);

    return len;
  }

  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM) {
    instanceData->err << "NPP_Write called";
  }

  if (instanceData->functionToFail == FUNCTION_NPP_WRITE) {
    return -1;
  }

  URLNotifyData* nd = static_cast<URLNotifyData*>(stream->notifyData);

  if (nd && nd->writeCallback) {
    NPVariant args[1];
    STRINGN_TO_NPVARIANT(stream->url, strlen(stream->url), args[0]);

    NPVariant result;
    NPN_InvokeDefault(instance, nd->writeCallback, args, 1, &result);
    NPN_ReleaseVariantValue(&result);
  }

  if (nd && nd != &kNotifyData) {
    uint32_t newsize = nd->size + len;
    nd->data = (char*) realloc(nd->data, newsize);
    memcpy(nd->data + nd->size, buffer, len);
    nd->size = newsize;
    return len;
  }

  if (instanceData->closeStream) {
    instanceData->closeStream = false;
    if (instanceData->testrange != nullptr) {
      NPN_RequestRead(stream, instanceData->testrange);
    }
    NPN_DestroyStream(instance, stream, NPRES_USER_BREAK);
  }
  else if (instanceData->streamMode == NP_SEEK &&
      stream->end != 0 &&
      stream->end == ((uint32_t)instanceData->streamBufSize + len)) {
    // If the complete stream has been written, and we're doing a seek test,
    // then call NPN_RequestRead.
    // prevent recursion
    instanceData->streamMode = NP_NORMAL;

    if (instanceData->testrange != nullptr) {
      NPError err = NPN_RequestRead(stream, instanceData->testrange);
      if (err != NPERR_NO_ERROR) {
        instanceData->err << "NPN_RequestRead returned error %d" << err;
      }
      printf("called NPN_RequestRead, return %d\n", err);
    }
  }

  char* streamBuf = reinterpret_cast<char *>(instanceData->streamBuf);
  if (offset + len <= instanceData->streamBufSize) {
    if (memcmp(buffer, streamBuf + offset, len)) {
      instanceData->err <<
          "Error: data written from NPN_RequestRead doesn't match";
    }
    else {
      printf("data matches!\n");
    }
    TestRange* range = instanceData->testrange;
    bool stillwaiting = false;
    while(range != nullptr) {
      if (offset == range->offset &&
        (uint32_t)len == range->length) {
        range->waiting = false;
      }
      if (range->waiting) stillwaiting = true;
      range = reinterpret_cast<TestRange*>(range->next);
    }
    if (!stillwaiting) {
      NPError err = NPN_DestroyStream(instance, stream, NPRES_DONE);
      if (err != NPERR_NO_ERROR) {
        instanceData->err << "Error: NPN_DestroyStream returned " << err;
      }
    }
  }
  else {
    if (instanceData->streamBufSize == 0) {
      instanceData->streamBuf = malloc(len + 1);
      streamBuf = reinterpret_cast<char *>(instanceData->streamBuf);
    }
    else {
      instanceData->streamBuf =
        realloc(reinterpret_cast<char *>(instanceData->streamBuf),
        instanceData->streamBufSize + len + 1);
      streamBuf = reinterpret_cast<char *>(instanceData->streamBuf);
    }
    memcpy(streamBuf + instanceData->streamBufSize, buffer, len);
    instanceData->streamBufSize = instanceData->streamBufSize + len;
    streamBuf[instanceData->streamBufSize] = '\0';
  }
  return len;
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
  size_t size;

  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM ||
      instanceData->functionToFail == FUNCTION_NPP_WRITE) {
    instanceData->err << "NPP_StreamAsFile called";
  }

  if (!fname)
    return;

  FILE *file = fopen(fname, "rb");
  if (file) {
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    instanceData->fileBuf = malloc((int32_t)size + 1);
    char* buf = reinterpret_cast<char *>(instanceData->fileBuf);
    fseek(file, 0, SEEK_SET);
    size_t sizeRead = fread(instanceData->fileBuf, 1, size, file);
    if (sizeRead != size) {
      printf("Unable to read data from file\n");
      instanceData->err << "Unable to read data from file " << fname;
    }
    fclose(file);
    buf[size] = '\0';
    instanceData->fileBufSize = (int32_t)size;
  }
  else {
    printf("Unable to open file\n");
    instanceData->err << "Unable to open file " << fname;
  }
}

void
NPP_Print(NPP instance, NPPrint* platformPrint)
{
}

int16_t
NPP_HandleEvent(NPP instance, void* event)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  return pluginHandleEvent(instanceData, event);
}

void
NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  URLNotifyData* ndata = static_cast<URLNotifyData*>(notifyData);

  if (&kNotifyData == ndata) {
    if (instanceData->frame.length() > 0) {
      sendBufferToFrame(instance);
    }
  }
  else if (!strcmp(ndata->cookie, "dynamic-cookie")) {
    if (ndata->notifyCallback) {
      NPVariant args[2];
      INT32_TO_NPVARIANT(reason, args[0]);
      if (ndata->data) {
        STRINGN_TO_NPVARIANT(ndata->data, ndata->size, args[1]);
      }
      else {
        STRINGN_TO_NPVARIANT("", 0, args[1]);
      }

      NPVariant result;
      NPN_InvokeDefault(instance, ndata->notifyCallback, args, 2, &result);
      NPN_ReleaseVariantValue(&result);
    }

    // clean up the URLNotifyData
    if (ndata->writeCallback) {
      NPN_ReleaseObject(ndata->writeCallback);
    }
    if (ndata->notifyCallback) {
      NPN_ReleaseObject(ndata->notifyCallback);
    }
    if (ndata->redirectCallback) {
      NPN_ReleaseObject(ndata->redirectCallback);
    }
    free(ndata->data);
    delete ndata;
  }
  else {
    printf("ERROR! NPP_URLNotify called with wrong cookie\n");
    instanceData->err << "Error: NPP_URLNotify called with wrong cookie";
  }
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void* value)
{
  InstanceData* instanceData = (InstanceData*)instance->pdata;
  if (variable == NPPVpluginScriptableNPObject) {
    NPObject* object = instanceData->scriptableObject;
    NPN_RetainObject(object);
    *((NPObject**)value) = object;
    return NPERR_NO_ERROR;
  }
  if (variable == NPPVpluginNeedsXEmbed) {
    // Only relevant for X plugins
    // use 4-byte writes like some plugins may do
    *(uint32_t*)value = instanceData->hasWidget;
    return NPERR_NO_ERROR;
  }
  if (variable == NPPVpluginWantsAllNetworkStreams) {
    // use 4-byte writes like some plugins may do
    *(uint32_t*)value = instanceData->wantsAllStreams;
    return NPERR_NO_ERROR;
  }

  return NPERR_GENERIC_ERROR;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void* value)
{
  if (variable == NPNVprivateModeBool) {
    InstanceData* instanceData = (InstanceData*)(instance->pdata);
    instanceData->lastReportedPrivateModeState = bool(*static_cast<NPBool*>(value));
    return NPERR_NO_ERROR;
  }
  if (variable == NPNVmuteAudioBool) {
    InstanceData* instanceData = (InstanceData*)(instance->pdata);
    instanceData->audioMuted = bool(*static_cast<NPBool*>(value));
    return NPERR_NO_ERROR;
  }
  if (variable == NPNVCSSZoomFactor) {
    InstanceData* instanceData = (InstanceData*)(instance->pdata);
    instanceData->cssZoomFactor = *static_cast<double*>(value);
    return NPERR_NO_ERROR;
  }
  return NPERR_GENERIC_ERROR;
}

void
NPP_URLRedirectNotify(NPP instance, const char* url, int32_t status, void* notifyData)
{
  if (notifyData) {
    URLNotifyData* nd = static_cast<URLNotifyData*>(notifyData);
    if (nd->redirectCallback) {
      NPVariant args[2];
      STRINGN_TO_NPVARIANT(url, strlen(url), args[0]);
      INT32_TO_NPVARIANT(status, args[1]);

      NPVariant result;
      NPN_InvokeDefault(instance, nd->redirectCallback, args, 2, &result);
      NPN_ReleaseVariantValue(&result);
    }
    NPN_URLRedirectResponse(instance, notifyData, nd->allowRedirects);
    return;
  }
  NPN_URLRedirectResponse(instance, notifyData, true);
}

NPError
NPP_ClearSiteData(const char* site, uint64_t flags, uint64_t maxAge)
{
  if (!sSitesWithData)
    return NPERR_NO_ERROR;

  // Error condition: no support for clear-by-age
  if (!sClearByAgeSupported && maxAge != uint64_t(int64_t(-1)))
    return NPERR_TIME_RANGE_NOT_SUPPORTED;

  // Iterate over list and remove matches
  list<siteData>::iterator iter = sSitesWithData->begin();
  list<siteData>::iterator end = sSitesWithData->end();
  while (iter != end) {
    const siteData& data = *iter;
    list<siteData>::iterator next = iter;
    ++next;
    if ((!site || data.site.compare(site) == 0) &&
        (flags == NP_CLEAR_ALL || data.flags & flags) &&
        data.age <= maxAge) {
      sSitesWithData->erase(iter);
    }
    iter = next;
  }

  return NPERR_NO_ERROR;
}

char**
NPP_GetSitesWithData()
{
  int length = 0;
  char** result;

  if (sSitesWithData)
    length = sSitesWithData->size();

  // Allocate the maximum possible size the list could be.
  result = static_cast<char**>(NPN_MemAlloc((length + 1) * sizeof(char*)));
  result[length] = nullptr;

  if (length == 0) {
    // Represent the no site data case as an array of length 1 with a nullptr
    // entry.
    return result;
  }

  // Iterate the list of stored data, and build a list of strings.
  list<string> sites;
  {
    list<siteData>::iterator iter = sSitesWithData->begin();
    list<siteData>::iterator end = sSitesWithData->end();
    for (; iter != end; ++iter) {
      const siteData& data = *iter;
      sites.push_back(data.site);
    }
  }

  // Remove duplicate strings.
  sites.sort();
  sites.unique();

  // Add strings to the result array, and null terminate.
  {
    int i = 0;
    list<string>::iterator iter = sites.begin();
    list<string>::iterator end = sites.end();
    for (; iter != end; ++iter, ++i) {
      const string& site = *iter;
      result[i] = static_cast<char*>(NPN_MemAlloc(site.length() + 1));
      memcpy(result[i], site.c_str(), site.length() + 1);
    }
  }
  result[sites.size()] = nullptr;

  return result;
}

//
// npapi browser functions
//

bool
NPN_SetProperty(NPP instance, NPObject* obj, NPIdentifier propertyName, const NPVariant* value)
{
  return sBrowserFuncs->setproperty(instance, obj, propertyName, value);
}

NPIdentifier
NPN_GetIntIdentifier(int32_t intid)
{
  return sBrowserFuncs->getintidentifier(intid);
}

NPIdentifier
NPN_GetStringIdentifier(const NPUTF8* name)
{
  return sBrowserFuncs->getstringidentifier(name);
}

void
NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
  return sBrowserFuncs->getstringidentifiers(names, nameCount, identifiers);
}

bool
NPN_IdentifierIsString(NPIdentifier identifier)
{
  return sBrowserFuncs->identifierisstring(identifier);
}

NPUTF8*
NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
  return sBrowserFuncs->utf8fromidentifier(identifier);
}

int32_t
NPN_IntFromIdentifier(NPIdentifier identifier)
{
  return sBrowserFuncs->intfromidentifier(identifier);
}

NPError
NPN_GetValue(NPP instance, NPNVariable variable, void* value)
{
  return sBrowserFuncs->getvalue(instance, variable, value);
}

NPError
NPN_SetValue(NPP instance, NPPVariable variable, void* value)
{
  return sBrowserFuncs->setvalue(instance, variable, value);
}

void
NPN_InvalidateRect(NPP instance, NPRect* rect)
{
  sBrowserFuncs->invalidaterect(instance, rect);
}

bool
NPN_HasProperty(NPP instance, NPObject* obj, NPIdentifier propertyName)
{
  return sBrowserFuncs->hasproperty(instance, obj, propertyName);
}

NPObject*
NPN_CreateObject(NPP instance, NPClass* aClass)
{
  return sBrowserFuncs->createobject(instance, aClass);
}

bool
NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return sBrowserFuncs->invoke(npp, obj, methodName, args, argCount, result);
}

bool
NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return sBrowserFuncs->invokeDefault(npp, obj, args, argCount, result);
}

bool
NPN_Construct(NPP npp, NPObject* npobj, const NPVariant* args,
	      uint32_t argCount, NPVariant* result)
{
  return sBrowserFuncs->construct(npp, npobj, args, argCount, result);
}

const char*
NPN_UserAgent(NPP instance)
{
  return sBrowserFuncs->uagent(instance);
}

NPObject*
NPN_RetainObject(NPObject* obj)
{
  return sBrowserFuncs->retainobject(obj);
}

void
NPN_ReleaseObject(NPObject* obj)
{
  return sBrowserFuncs->releaseobject(obj);
}

void*
NPN_MemAlloc(uint32_t size)
{
  return sBrowserFuncs->memalloc(size);
}

char*
NPN_StrDup(const char* str)
{
  return strcpy((char*)sBrowserFuncs->memalloc(strlen(str) + 1), str);
}

void
NPN_MemFree(void* ptr)
{
  return sBrowserFuncs->memfree(ptr);
}

uint32_t
NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
  return sBrowserFuncs->scheduletimer(instance, interval, repeat, timerFunc);
}

void
NPN_UnscheduleTimer(NPP instance, uint32_t timerID)
{
  return sBrowserFuncs->unscheduletimer(instance, timerID);
}

void
NPN_ReleaseVariantValue(NPVariant *variant)
{
  return sBrowserFuncs->releasevariantvalue(variant);
}

NPError
NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
  return sBrowserFuncs->geturlnotify(instance, url, target, notifyData);
}

NPError
NPN_GetURL(NPP instance, const char* url, const char* target)
{
  return sBrowserFuncs->geturl(instance, url, target);
}

NPError
NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
  return sBrowserFuncs->requestread(stream, rangeList);
}

NPError
NPN_PostURLNotify(NPP instance, const char* url,
                  const char* target, uint32_t len,
                  const char* buf, NPBool file, void* notifyData)
{
  return sBrowserFuncs->posturlnotify(instance, url, target, len, buf, file, notifyData);
}

NPError
NPN_PostURL(NPP instance, const char *url,
                    const char *target, uint32_t len,
                    const char *buf, NPBool file)
{
  return sBrowserFuncs->posturl(instance, url, target, len, buf, file);
}

NPError
NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
  return sBrowserFuncs->destroystream(instance, stream, reason);
}

NPError
NPN_NewStream(NPP instance,
              NPMIMEType  type,
              const char* target,
              NPStream**  stream)
{
  return sBrowserFuncs->newstream(instance, type, target, stream);
}

int32_t
NPN_Write(NPP instance,
          NPStream* stream,
          int32_t len,
          void* buf)
{
  return sBrowserFuncs->write(instance, stream, len, buf);
}

bool
NPN_Enumerate(NPP instance,
              NPObject *npobj,
              NPIdentifier **identifiers,
              uint32_t *identifierCount)
{
  return sBrowserFuncs->enumerate(instance, npobj, identifiers,
      identifierCount);
}

bool
NPN_GetProperty(NPP instance,
                NPObject *npobj,
                NPIdentifier propertyName,
                NPVariant *result)
{
  return sBrowserFuncs->getproperty(instance, npobj, propertyName, result);
}

bool
NPN_Evaluate(NPP instance, NPObject *npobj, NPString *script, NPVariant *result)
{
  return sBrowserFuncs->evaluate(instance, npobj, script, result);
}

void
NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
  return sBrowserFuncs->setexception(npobj, message);
}

NPBool
NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace)
{
  return sBrowserFuncs->convertpoint(instance, sourceX, sourceY, sourceSpace, destX, destY, destSpace);
}

NPError
NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len)
{
  return sBrowserFuncs->setvalueforurl(instance, variable, url, value, len);
}

NPError
NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len)
{
  return sBrowserFuncs->getvalueforurl(instance, variable, url, value, len);
}

void
NPN_PluginThreadAsyncCall(NPP plugin, void (*func)(void*), void* userdata)
{
  return sBrowserFuncs->pluginthreadasynccall(plugin, func, userdata);
}

void
NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow)
{
  return sBrowserFuncs->urlredirectresponse(instance, notifyData, allow);
}

NPError
NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format,
                     void *initData, NPAsyncSurface *surface)
{
  return sBrowserFuncs->initasyncsurface(instance, size, format, initData, surface);
}

NPError
NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface)
{
  return sBrowserFuncs->finalizeasyncsurface(instance, surface);
}

void
NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed)
{
  sBrowserFuncs->setcurrentasyncsurface(instance, surface, changed);
}

//
// npruntime object functions
//

NPObject*
scriptableAllocate(NPP npp, NPClass* aClass)
{
  TestNPObject* object = (TestNPObject*)NPN_MemAlloc(sizeof(TestNPObject));
  if (!object)
    return nullptr;
  memset(object, 0, sizeof(TestNPObject));
  return object;
}

void
scriptableDeallocate(NPObject* npobj)
{
  NPN_MemFree(npobj);
}

void
scriptableInvalidate(NPObject* npobj)
{
}

bool
scriptableHasMethod(NPObject* npobj, NPIdentifier name)
{
  for (int i = 0; i < int(MOZ_ARRAY_LENGTH(sPluginMethodIdentifiers)); i++) {
    if (name == sPluginMethodIdentifiers[i])
      return true;
  }
  return false;
}

bool
scriptableInvoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  if (id->throwOnNextInvoke) {
    id->throwOnNextInvoke = false;
    if (argCount == 0) {
      NPN_SetException(npobj, nullptr);
    }
    else {
      for (uint32_t i = 0; i < argCount; i++) {
        const NPString* argstr = &NPVARIANT_TO_STRING(args[i]);
        NPN_SetException(npobj, argstr->UTF8Characters);
      }
    }
    return false;
  }

  for (int i = 0; i < int(MOZ_ARRAY_LENGTH(sPluginMethodIdentifiers)); i++) {
    if (name == sPluginMethodIdentifiers[i])
      return sPluginMethodFunctions[i](npobj, args, argCount, result);
  }
  return false;
}

bool
scriptableHasProperty(NPObject* npobj, NPIdentifier name)
{
  if (NPN_IdentifierIsString(name)) {
    NPUTF8 *asUTF8 = NPN_UTF8FromIdentifier(name);
    if (NPN_GetStringIdentifier(asUTF8) != name) {
      Crash();
    }
    NPN_MemFree(asUTF8);
  }
  else {
    if (NPN_GetIntIdentifier(NPN_IntFromIdentifier(name)) != name) {
      Crash();
    }
  }
  for (int i = 0; i < int(MOZ_ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      return true;
    }
  }
  return false;
}

bool
scriptableGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
  for (int i = 0; i < int(MOZ_ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      DuplicateNPVariant(*result, sPluginPropertyValues[i]);
      return true;
    }
  }
  return false;
}

bool
scriptableSetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value)
{
  for (int i = 0; i < int(MOZ_ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      NPN_ReleaseVariantValue(&sPluginPropertyValues[i]);
      DuplicateNPVariant(sPluginPropertyValues[i], *value);
      return true;
    }
  }
  return false;
}

bool
scriptableRemoveProperty(NPObject* npobj, NPIdentifier name)
{
  for (int i = 0; i < int(MOZ_ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      NPN_ReleaseVariantValue(&sPluginPropertyValues[i]);

      // Avoid double frees (see test_propertyAndMethod.html, which deletes a
      // property that doesn't exist).
      VOID_TO_NPVARIANT(sPluginPropertyValues[i]);
      return true;
    }
  }
  return false;
}

bool
scriptableEnumerate(NPObject* npobj, NPIdentifier** identifier, uint32_t* count)
{
  const int bufsize = sizeof(NPIdentifier) * MOZ_ARRAY_LENGTH(sPluginMethodIdentifierNames);
  NPIdentifier* ids = (NPIdentifier*) NPN_MemAlloc(bufsize);
  if (!ids)
    return false;

  memcpy(ids, sPluginMethodIdentifiers, bufsize);
  *identifier = ids;
  *count = MOZ_ARRAY_LENGTH(sPluginMethodIdentifierNames);
  return true;
}

bool
scriptableConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  return false;
}

//
// test functions
//

static bool
compareVariants(NPP instance, const NPVariant* var1, const NPVariant* var2)
{
  bool success = true;
  InstanceData* id = static_cast<InstanceData*>(instance->pdata);
  if (var1->type != var2->type) {
    id->err << "Variant types don't match; got " << var1->type <<
        " expected " << var2->type;
    return false;
  }

  // Cast var1->type from NPVariantType to int to avoid compiler warnings about
  // not needing a default case when we have cases for every enum value.
  switch (static_cast<int>(var1->type)) {
    case NPVariantType_Int32: {
        int32_t result = NPVARIANT_TO_INT32(*var1);
        int32_t expected = NPVARIANT_TO_INT32(*var2);
        if (result != expected) {
          id->err << "Variant values don't match; got " << result <<
              " expected " << expected;
          success = false;
        }
        break;
      }
    case NPVariantType_Double: {
        double result = NPVARIANT_TO_DOUBLE(*var1);
        double expected = NPVARIANT_TO_DOUBLE(*var2);
        if (result != expected) {
          id->err << "Variant values don't match (double)";
          success = false;
        }
        break;
      }
    case NPVariantType_Void: {
        // void values are always equivalent
        break;
      }
    case NPVariantType_Null: {
        // null values are always equivalent
        break;
      }
    case NPVariantType_Bool: {
        bool result = NPVARIANT_TO_BOOLEAN(*var1);
        bool expected = NPVARIANT_TO_BOOLEAN(*var2);
        if (result != expected) {
          id->err << "Variant values don't match (bool)";
          success = false;
        }
        break;
      }
    case NPVariantType_String: {
        const NPString* result = &NPVARIANT_TO_STRING(*var1);
        const NPString* expected = &NPVARIANT_TO_STRING(*var2);
        if (strcmp(result->UTF8Characters, expected->UTF8Characters) ||
            strlen(result->UTF8Characters) != strlen(expected->UTF8Characters)) {
          id->err << "Variant values don't match; got " <<
              result->UTF8Characters << " expected " <<
              expected->UTF8Characters;
          success = false;
        }
        break;
      }
    case NPVariantType_Object: {
        uint32_t i, identifierCount = 0;
        NPIdentifier* identifiers;
        NPObject* result = NPVARIANT_TO_OBJECT(*var1);
        NPObject* expected = NPVARIANT_TO_OBJECT(*var2);
        bool enumerate_result = NPN_Enumerate(instance, expected,
            &identifiers, &identifierCount);
        if (!enumerate_result) {
          id->err << "NPN_Enumerate failed";
          success = false;
        }
        for (i = 0; i < identifierCount; i++) {
          NPVariant resultVariant, expectedVariant;
          if (!NPN_GetProperty(instance, expected, identifiers[i],
              &expectedVariant)) {
            id->err << "NPN_GetProperty returned false";
            success = false;
          }
          else {
            if (!NPN_HasProperty(instance, result, identifiers[i])) {
              id->err << "NPN_HasProperty returned false";
              success = false;
            }
            else {
              if (!NPN_GetProperty(instance, result, identifiers[i],
              &resultVariant)) {
                id->err << "NPN_GetProperty 2 returned false";
                success = false;
              }
              else {
                success = compareVariants(instance, &resultVariant,
                    &expectedVariant);
                NPN_ReleaseVariantValue(&expectedVariant);
              }
            }
            NPN_ReleaseVariantValue(&resultVariant);
          }
        }
        NPN_MemFree(identifiers);
        break;
      }
    default:
      id->err << "Unknown variant type";
      success = false;
      MOZ_ASSERT_UNREACHABLE("Unknown variant type?!");
  }

  return success;
}

static bool
throwExceptionNextInvoke(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->throwOnNextInvoke = true;
  BOOLEAN_TO_NPVARIANT(true, *result);
  return true;
}

static bool
npnInvokeDefaultTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  bool success = false;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return false;

  NPIdentifier objectIdentifier = variantToIdentifier(args[0]);
  if (!objectIdentifier)
    return false;

  NPVariant objectVariant;
  if (NPN_GetProperty(npp, windowObject, objectIdentifier,
      &objectVariant)) {
    if (NPVARIANT_IS_OBJECT(objectVariant)) {
      NPObject* selfObject = NPVARIANT_TO_OBJECT(objectVariant);
      if (selfObject != nullptr) {
        NPVariant resultVariant;
        if (NPN_InvokeDefault(npp, selfObject, argCount > 1 ? &args[1] : nullptr,
            argCount - 1, &resultVariant)) {
          *result = resultVariant;
          success = true;
        }
      }
    }
    NPN_ReleaseVariantValue(&objectVariant);
  }

  NPN_ReleaseObject(windowObject);
  return success;
}

static bool
npnInvokeTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->err.str("");
  if (argCount < 2)
    return false;

  NPIdentifier function = variantToIdentifier(args[0]);
  if (!function)
    return false;

  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return false;

  NPVariant invokeResult;
  bool invokeReturn = NPN_Invoke(npp, windowObject, function,
      argCount > 2 ? &args[2] : nullptr, argCount - 2, &invokeResult);

  bool compareResult = compareVariants(npp, &invokeResult, &args[1]);

  NPN_ReleaseObject(windowObject);
  NPN_ReleaseVariantValue(&invokeResult);
  BOOLEAN_TO_NPVARIANT(invokeReturn && compareResult, *result);
  return true;
}

static bool
npnEvaluateTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  bool success = false;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (argCount != 1)
    return false;

  if (!NPVARIANT_IS_STRING(args[0]))
    return false;

  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return false;

  success = NPN_Evaluate(npp, windowObject, (NPString*)&NPVARIANT_TO_STRING(args[0]), result);

  NPN_ReleaseObject(windowObject);
  return success;
}

static bool
setUndefinedValueTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  NPError err = NPN_SetValue(npp, (NPPVariable)0x0, 0x0);
  BOOLEAN_TO_NPVARIANT((err == NPERR_NO_ERROR), *result);
  return true;
}

static bool
identifierToStringTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  NPIdentifier identifier = variantToIdentifier(args[0]);
  if (!identifier)
    return false;

  NPUTF8* utf8String = NPN_UTF8FromIdentifier(identifier);
  if (!utf8String)
    return false;
  STRINGZ_TO_NPVARIANT(utf8String, *result);
  return true;
}

static bool
queryPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPBool pms = false;
  NPN_GetValue(static_cast<TestNPObject*>(npobj)->npp, NPNVprivateModeBool, &pms);
  BOOLEAN_TO_NPVARIANT(pms, *result);
  return true;
}

static bool
lastReportedPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  BOOLEAN_TO_NPVARIANT(id->lastReportedPrivateModeState, *result);
  return true;
}

static bool
hasWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  BOOLEAN_TO_NPVARIANT(id->hasWidget, *result);
  return true;
}

static bool
getEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t edge = NPVARIANT_TO_INT32(args[0]);
  if (edge < EDGE_LEFT || edge > EDGE_BOTTOM)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  int32_t r = pluginGetEdge(id, RectEdge(edge));
  if (r == NPTEST_INT32_ERROR)
    return false;
  INT32_TO_NPVARIANT(r, *result);
  return true;
}

static bool
getClipRegionRectCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  int32_t r = pluginGetClipRegionRectCount(id);
  if (r == NPTEST_INT32_ERROR)
    return false;
  INT32_TO_NPVARIANT(r, *result);
  return true;
}

static bool
getClipRegionRectEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 2)
    return false;
  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t rectIndex = NPVARIANT_TO_INT32(args[0]);
  if (rectIndex < 0)
    return false;
  if (!NPVARIANT_IS_INT32(args[1]))
    return false;
  int32_t edge = NPVARIANT_TO_INT32(args[1]);
  if (edge < EDGE_LEFT || edge > EDGE_BOTTOM)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  int32_t r = pluginGetClipRegionRectEdge(id, rectIndex, RectEdge(edge));
  if (r == NPTEST_INT32_ERROR)
    return false;
  INT32_TO_NPVARIANT(r, *result);
  return true;
}

static bool
startWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  if (sWatchingInstanceCount)
    return false;

  sWatchingInstanceCount = true;
  sInstanceCount = 0;
  ++sCurrentInstanceCountWatchGeneration;
  return true;
}

static bool
getInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  if (!sWatchingInstanceCount)
    return false;

  INT32_TO_NPVARIANT(sInstanceCount, *result);
  return true;
}

static bool
stopWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  if (!sWatchingInstanceCount)
    return false;

  sWatchingInstanceCount = false;
  return true;
}

static bool
getLastMouseX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->lastMouseX, *result);
  return true;
}

static bool
getLastMouseY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->lastMouseY, *result);
  return true;
}

static bool
getPaintCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->paintCount, *result);
  return true;
}

static bool
resetPaintCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->paintCount = 0;
  return true;
}

static bool
getWidthAtLastPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->widthAtLastPaint, *result);
  return true;
}

static bool
setInvalidateDuringPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;

  if (!NPVARIANT_IS_BOOLEAN(args[0]))
    return false;
  bool doInvalidate = NPVARIANT_TO_BOOLEAN(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->invalidateDuringPaint = doInvalidate;
  return true;
}

static bool
setSlowPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;

  if (!NPVARIANT_IS_BOOLEAN(args[0]))
    return false;
  bool slow = NPVARIANT_TO_BOOLEAN(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->slowPaint = slow;
  return true;
}

static bool
getError(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  if (id->err.str().length() == 0) {
    char *outval = NPN_StrDup(SUCCESS_STRING);
    STRINGZ_TO_NPVARIANT(outval, *result);
  } else {
    char *outval = NPN_StrDup(id->err.str().c_str());
    STRINGZ_TO_NPVARIANT(outval, *result);
  }
  return true;
}

static bool
doInternalConsistencyCheck(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  string error;
  pluginDoInternalConsistencyCheck(id, error);
  NPUTF8* utf8String = (NPUTF8*)NPN_MemAlloc(error.length() + 1);
  if (!utf8String) {
    return false;
  }
  memcpy(utf8String, error.c_str(), error.length() + 1);
  STRINGZ_TO_NPVARIANT(utf8String, *result);
  return true;
}

static bool
convertPointX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 4)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t sourceSpace = NPVARIANT_TO_INT32(args[0]);

  if (!NPVARIANT_IS_INT32(args[1]))
    return false;
  double sourceX = static_cast<double>(NPVARIANT_TO_INT32(args[1]));

  if (!NPVARIANT_IS_INT32(args[2]))
    return false;
  double sourceY = static_cast<double>(NPVARIANT_TO_INT32(args[2]));

  if (!NPVARIANT_IS_INT32(args[3]))
    return false;
  int32_t destSpace = NPVARIANT_TO_INT32(args[3]);

  double resultX, resultY;
  NPN_ConvertPoint(npp, sourceX, sourceY, (NPCoordinateSpace)sourceSpace, &resultX, &resultY, (NPCoordinateSpace)destSpace);

  DOUBLE_TO_NPVARIANT(resultX, *result);
  return true;
}

static bool
convertPointY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 4)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t sourceSpace = NPVARIANT_TO_INT32(args[0]);

  if (!NPVARIANT_IS_INT32(args[1]))
    return false;
  double sourceX = static_cast<double>(NPVARIANT_TO_INT32(args[1]));

  if (!NPVARIANT_IS_INT32(args[2]))
    return false;
  double sourceY = static_cast<double>(NPVARIANT_TO_INT32(args[2]));

  if (!NPVARIANT_IS_INT32(args[3]))
    return false;
  int32_t destSpace = NPVARIANT_TO_INT32(args[3]);

  double resultX, resultY;
  NPN_ConvertPoint(npp, sourceX, sourceY, (NPCoordinateSpace)sourceSpace, &resultX, &resultY, (NPCoordinateSpace)destSpace);

  DOUBLE_TO_NPVARIANT(resultY, *result);
  return true;
}

static bool
streamTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  // .streamTest(url, doPost, postData, writeCallback, notifyCallback, redirectCallback, allowRedirects, postFile = false)
  if (!(7 <= argCount && argCount <= 8))
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_STRING(args[0]))
    return false;
  NPString url = NPVARIANT_TO_STRING(args[0]);

  if (!NPVARIANT_IS_BOOLEAN(args[1]))
    return false;
  bool doPost = NPVARIANT_TO_BOOLEAN(args[1]);

  NPString postData = { nullptr, 0 };
  if (NPVARIANT_IS_STRING(args[2])) {
    postData = NPVARIANT_TO_STRING(args[2]);
  }
  else {
    if (!NPVARIANT_IS_NULL(args[2])) {
      return false;
    }
  }

  NPObject* writeCallback = nullptr;
  if (NPVARIANT_IS_OBJECT(args[3])) {
    writeCallback = NPVARIANT_TO_OBJECT(args[3]);
  }
  else {
    if (!NPVARIANT_IS_NULL(args[3])) {
      return false;
    }
  }

  NPObject* notifyCallback = nullptr;
  if (NPVARIANT_IS_OBJECT(args[4])) {
    notifyCallback = NPVARIANT_TO_OBJECT(args[4]);
  }
  else {
    if (!NPVARIANT_IS_NULL(args[4])) {
      return false;
    }
  }

  NPObject* redirectCallback = nullptr;
  if (NPVARIANT_IS_OBJECT(args[5])) {
    redirectCallback = NPVARIANT_TO_OBJECT(args[5]);
  }
  else {
    if (!NPVARIANT_IS_NULL(args[5])) {
      return false;
    }
  }

  if (!NPVARIANT_IS_BOOLEAN(args[6]))
    return false;
  bool allowRedirects = NPVARIANT_TO_BOOLEAN(args[6]);

  bool postFile = false;
  if (argCount >= 8) {
    if (!NPVARIANT_IS_BOOLEAN(args[7])) {
      return false;
    }
    postFile = NPVARIANT_TO_BOOLEAN(args[7]);
  }

  URLNotifyData* ndata = new URLNotifyData;
  ndata->cookie = "dynamic-cookie";
  ndata->writeCallback = writeCallback;
  ndata->notifyCallback = notifyCallback;
  ndata->redirectCallback = redirectCallback;
  ndata->size = 0;
  ndata->data = nullptr;
  ndata->allowRedirects = allowRedirects;

  /* null-terminate "url" */
  char* urlstr = (char*) malloc(url.UTF8Length + 1);
  strncpy(urlstr, url.UTF8Characters, url.UTF8Length);
  urlstr[url.UTF8Length] = '\0';

  NPError err;
  if (doPost) {
    err = NPN_PostURLNotify(npp, urlstr, nullptr,
                            postData.UTF8Length, postData.UTF8Characters,
                            postFile, ndata);
  }
  else {
    err = NPN_GetURLNotify(npp, urlstr, nullptr, ndata);
  }

  free(urlstr);

  if (NPERR_NO_ERROR == err) {
    if (ndata->writeCallback) {
      NPN_RetainObject(ndata->writeCallback);
    }
    if (ndata->notifyCallback) {
      NPN_RetainObject(ndata->notifyCallback);
    }
    if (ndata->redirectCallback) {
      NPN_RetainObject(ndata->redirectCallback);
    }
    BOOLEAN_TO_NPVARIANT(true, *result);
  }
  else {
    delete ndata;
    BOOLEAN_TO_NPVARIANT(false, *result);
  }

  return true;
}

static bool
postFileToURLTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (1 != argCount)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  string url;
  {
    if (!NPVARIANT_IS_STRING(args[0]))
      return false;
    NPString npurl = NPVARIANT_TO_STRING(args[0]);
    // make a copy to ensure that the url string is null-terminated
    url = string(npurl.UTF8Characters, npurl.UTF8Length);
  }


  NPError err;
  {
    string buf("/path/to/file");
    err = NPN_PostURL(npp,
                      url.c_str(),
                      nullptr /* target */,
                      buf.length(), buf.c_str(),
                      true /* file */);
  }

  BOOLEAN_TO_NPVARIANT(NPERR_NO_ERROR == err, *result);
  return true;
}

static bool
setPluginWantsAllStreams(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (1 != argCount)
    return false;

  if (!NPVARIANT_IS_BOOLEAN(args[0]))
    return false;
  bool wantsAllStreams = NPVARIANT_TO_BOOLEAN(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  id->wantsAllStreams = wantsAllStreams;

  return true;
}

static bool
crashPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  IntentionalCrash();
  VOID_TO_NPVARIANT(*result);
  return true;
}

static bool
crashOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  id->crashOnDestroy = true;
  VOID_TO_NPVARIANT(*result);
  return true;
}

static bool
setColor(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  if (!NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* str = &NPVARIANT_TO_STRING(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  id->scriptableObject->drawColor =
    parseHexColor(str->UTF8Characters, str->UTF8Length);

  NPRect r;
  r.left = 0;
  r.top = 0;
  r.right = id->window.width;
  r.bottom = id->window.height;
  if (id->asyncDrawing == AD_NONE) {
    NPN_InvalidateRect(npp, &r);
  } else if (id->asyncDrawing == AD_BITMAP) {
    drawAsyncBitmapColor(id);
  }

  VOID_TO_NPVARIANT(*result);
  return true;
}

void notifyDidPaint(InstanceData* instanceData)
{
  ++instanceData->paintCount;
  instanceData->widthAtLastPaint = instanceData->window.width;

  if (instanceData->invalidateDuringPaint) {
    NPRect r;
    r.left = 0;
    r.top = 0;
    r.right = instanceData->window.width;
    r.bottom = instanceData->window.height;
    NPN_InvalidateRect(instanceData->npp, &r);
  }

  if (instanceData->slowPaint) {
    XPSleep(1);
  }

  if (instanceData->runScriptOnPaint) {
    NPObject* o = nullptr;
    NPN_GetValue(instanceData->npp, NPNVPluginElementNPObject, &o);
    if (o) {
      NPVariant param;
      STRINGZ_TO_NPVARIANT("paintscript", param);
      NPVariant result;
      NPN_Invoke(instanceData->npp, o, NPN_GetStringIdentifier("getAttribute"),
                 &param, 1, &result);

      if (NPVARIANT_IS_STRING(result)) {
        NPObject* windowObject;
        NPN_GetValue(instanceData->npp, NPNVWindowNPObject, &windowObject);
        if (windowObject) {
          NPVariant evalResult;
          NPN_Evaluate(instanceData->npp, windowObject,
                       (NPString*)&NPVARIANT_TO_STRING(result), &evalResult);
          NPN_ReleaseVariantValue(&evalResult);
          NPN_ReleaseObject(windowObject);
        }
      }

      NPN_ReleaseVariantValue(&result);
      NPN_ReleaseObject(o);
    }
  }
}

static const NPClass kTestSharedNPClass = {
  NP_CLASS_STRUCT_VERSION,
  // Everything else is nullptr
};

static bool getJavaCodebase(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  char *outval = NPN_StrDup(id->javaCodebase.c_str());
  STRINGZ_TO_NPVARIANT(outval, *result);
  return true;
}

static bool getObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  NPObject* o = NPN_CreateObject(npp,
                                 const_cast<NPClass*>(&kTestSharedNPClass));
  if (!o)
    return false;

  OBJECT_TO_NPVARIANT(o, *result);
  return true;
}

static bool checkObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  VOID_TO_NPVARIANT(*result);

  if (1 != argCount)
    return false;

  if (!NPVARIANT_IS_OBJECT(args[0]))
    return false;

  NPObject* o = NPVARIANT_TO_OBJECT(args[0]);

  BOOLEAN_TO_NPVARIANT(o->_class == &kTestSharedNPClass, *result);
  return true;
}

static bool enableFPExceptions(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  VOID_TO_NPVARIANT(*result);

#if defined(XP_WIN) && defined(_M_IX86)
  _control87(0, _MCW_EM);
  return true;
#else
  return false;
#endif
}

static void timerCallback(NPP npp, uint32_t timerID)
{
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  currentTimerEventCount++;
  timerEvent event = timerEvents[currentTimerEventCount];

  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return;

  NPVariant rval;
  if (timerID != id->timerID[event.timerIdReceive]) {
    id->timerTestResult = false;
  }

  if (currentTimerEventCount == totalTimerEvents - 1) {
    NPVariant arg;
    BOOLEAN_TO_NPVARIANT(id->timerTestResult, arg);
    NPN_Invoke(npp, windowObject, NPN_GetStringIdentifier(id->timerTestScriptCallback.c_str()), &arg, 1, &rval);
    NPN_ReleaseVariantValue(&arg);
  }

  NPN_ReleaseObject(windowObject);

  if (event.timerIdSchedule > -1) {
    id->timerID[event.timerIdSchedule] = NPN_ScheduleTimer(npp, event.timerInterval, event.timerRepeat, timerCallback);
  }
  if (event.timerIdUnschedule > -1) {
    NPN_UnscheduleTimer(npp, id->timerID[event.timerIdUnschedule]);
  }
}

static bool
timerTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  currentTimerEventCount = 0;

  if (argCount < 1 || !NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* argstr = &NPVARIANT_TO_STRING(args[0]);
  id->timerTestScriptCallback = argstr->UTF8Characters;

  id->timerTestResult = true;
  timerEvent event = timerEvents[currentTimerEventCount];

  id->timerID[event.timerIdSchedule] = NPN_ScheduleTimer(npp, event.timerInterval, event.timerRepeat, timerCallback);

  return id->timerID[event.timerIdSchedule] != 0;
}

#ifdef XP_WIN
void
ThreadProc(void* cookie)
#else
void*
ThreadProc(void* cookie)
#endif
{
  NPObject* npobj = (NPObject*)cookie;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->asyncTestPhase = 1;
  NPN_PluginThreadAsyncCall(npp, asyncCallback, (void*)npobj);
#ifndef XP_WIN
  return nullptr;
#endif
}

void
asyncCallback(void* cookie)
{
  NPObject* npobj = (NPObject*)cookie;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  switch (id->asyncTestPhase) {
    // async callback triggered from same thread
    case 0:
#ifdef XP_WIN
      if (_beginthread(ThreadProc, 0, (void*)npobj) == -1)
        id->asyncCallbackResult = false;
#else
      pthread_t tid;
      if (pthread_create(&tid, 0, ThreadProc, (void*)npobj))
        id->asyncCallbackResult = false;
#endif
      break;

    // async callback triggered from different thread
    default:
      NPObject* windowObject;
      NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
      if (!windowObject)
        return;
      NPVariant arg, rval;
      BOOLEAN_TO_NPVARIANT(id->asyncCallbackResult, arg);
      NPN_Invoke(npp, windowObject, NPN_GetStringIdentifier(id->asyncTestScriptCallback.c_str()), &arg, 1, &rval);
      NPN_ReleaseVariantValue(&arg);
      NPN_ReleaseObject(windowObject);
      break;
  }
}

static bool
asyncCallbackTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  if (argCount < 1 || !NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* argstr = &NPVARIANT_TO_STRING(args[0]);
  id->asyncTestScriptCallback = argstr->UTF8Characters;

  id->asyncTestPhase = 0;
  id->asyncCallbackResult = true;
  NPN_PluginThreadAsyncCall(npp, asyncCallback, (void*)npobj);

  return true;
}

static bool
GCRaceInvoke(NPObject*, NPIdentifier, const NPVariant*, uint32_t, NPVariant*)
{
  return false;
}

static bool
GCRaceInvokeDefault(NPObject* o, const NPVariant* args, uint32_t argCount,
		    NPVariant* result)
{
  if (1 != argCount || !NPVARIANT_IS_INT32(args[0]) ||
      35 != NPVARIANT_TO_INT32(args[0]))
    return false;

  return true;
}

static const NPClass kGCRaceClass = {
  NP_CLASS_STRUCT_VERSION,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  GCRaceInvoke,
  GCRaceInvokeDefault,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr
};

struct GCRaceData
{
  GCRaceData(NPP npp, NPObject* callback, NPObject* localFunc)
    : npp_(npp)
    , callback_(callback)
    , localFunc_(localFunc)
  {
    NPN_RetainObject(callback_);
    NPN_RetainObject(localFunc_);
  }

  ~GCRaceData()
  {
    NPN_ReleaseObject(callback_);
    NPN_ReleaseObject(localFunc_);
  }

  NPP npp_;
  NPObject* callback_;
  NPObject* localFunc_;
};

static void
FinishGCRace(void* closure)
{
  GCRaceData* rd = static_cast<GCRaceData*>(closure);

  XPSleep(5);

  NPVariant arg;
  OBJECT_TO_NPVARIANT(rd->localFunc_, arg);

  NPVariant result;
  bool ok = NPN_InvokeDefault(rd->npp_, rd->callback_, &arg, 1, &result);
  if (!ok)
    return;

  NPN_ReleaseVariantValue(&result);
  delete rd;
}

bool
checkGCRace(NPObject* npobj, const NPVariant* args, uint32_t argCount,
	    NPVariant* result)
{
  if (1 != argCount || !NPVARIANT_IS_OBJECT(args[0]))
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  NPObject* localFunc =
    NPN_CreateObject(npp, const_cast<NPClass*>(&kGCRaceClass));

  GCRaceData* rd =
    new GCRaceData(npp, NPVARIANT_TO_OBJECT(args[0]), localFunc);
  NPN_PluginThreadAsyncCall(npp, FinishGCRace, rd);

  OBJECT_TO_NPVARIANT(localFunc, *result);
  return true;
}

bool
hangPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount,
           NPVariant* result)
{
  mozilla::NoteIntentionalCrash("plugin");

  bool busyHang = false;
  if ((argCount == 1) && NPVARIANT_IS_BOOLEAN(args[0])) {
    busyHang = NPVARIANT_TO_BOOLEAN(args[0]);
  }

  if (busyHang) {
    const time_t start = std::time(nullptr);
    while ((std::time(nullptr) - start) < 100000) {
      volatile int dummy = 0;
      for (int i=0; i<1000; ++i) {
        dummy++;
      }
    }
  } else {
#ifdef XP_WIN
  Sleep(100000000);
    Sleep(100000000);
#else
  pause();
    pause();
#endif
  }

  // NB: returning true here means that we weren't terminated, and
  // thus the hang detection/handling didn't work correctly.  The
  // test harness will succeed in calling this function, and the
  // test will fail.
  return true;
}

bool
stallPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount,
           NPVariant* result)
{
  uint32_t stallTimeSeconds = 0;
  if ((argCount == 1) && NPVARIANT_IS_INT32(args[0])) {
    stallTimeSeconds = (uint32_t) NPVARIANT_TO_INT32(args[0]);
  }

#ifdef XP_WIN
  Sleep(stallTimeSeconds * 1000U);
#else
  sleep(stallTimeSeconds);
#endif

  return true;
}

#if defined(MOZ_WIDGET_GTK)
bool
getClipboardText(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                 NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  string sel = pluginGetClipboardText(id);

  uint32_t len = sel.size();
  char* selCopy = static_cast<char*>(NPN_MemAlloc(1 + len));
  if (!selCopy)
    return false;

  memcpy(selCopy, sel.c_str(), len);
  selCopy[len] = '\0';

  STRINGN_TO_NPVARIANT(selCopy, len, *result);
  // *result owns str now

  return true;
}

bool
crashPluginInNestedLoop(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  return pluginCrashInNestedLoop(id);
}

bool
triggerXError(NPObject* npobj, const NPVariant* args,
              uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  return pluginTriggerXError(id);
}

bool
destroySharedGfxStuff(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  return pluginDestroySharedGfxStuff(id);
}

#else
bool
getClipboardText(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                 NPVariant* result)
{
  // XXX Not implemented!
  return false;
}

bool
crashPluginInNestedLoop(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  // XXX Not implemented!
  return false;
}

bool
triggerXError(NPObject* npobj, const NPVariant* args,
              uint32_t argCount, NPVariant* result)
{
  // XXX Not implemented!
  return false;
}

bool
destroySharedGfxStuff(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  // XXX Not implemented!
  return false;
}
#endif

#if defined(XP_WIN)
bool
nativeWidgetIsVisible(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  bool visible = pluginNativeWidgetIsVisible(id);
  BOOLEAN_TO_NPVARIANT(visible, *result);
  return true;
}
#else
bool
nativeWidgetIsVisible(NPObject* npobj, const NPVariant* args,
                      uint32_t argCount, NPVariant* result)
{
  // XXX Not implemented!
  return false;
}
#endif

bool
getLastCompositionText(NPObject* npobj, const NPVariant* args,
                       uint32_t argCount, NPVariant* result)
{
#ifdef XP_WIN
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  char *outval = NPN_StrDup(id->lastComposition.c_str());
  STRINGZ_TO_NPVARIANT(outval, *result);
  return true;
#else
  // XXX not implemented
  return false;
#endif
}

bool
scriptableInvokeDefault(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  ostringstream value;
  value << sPluginName;
  for (uint32_t i = 0; i < argCount; i++) {
    switch(args[i].type) {
      case NPVariantType_Int32:
        value << ";" << NPVARIANT_TO_INT32(args[i]);
        break;
      case NPVariantType_String: {
        const NPString* argstr = &NPVARIANT_TO_STRING(args[i]);
        value << ";" << argstr->UTF8Characters;
        break;
      }
      case NPVariantType_Void:
        value << ";undefined";
        break;
      case NPVariantType_Null:
        value << ";null";
        break;
      default:
        value << ";other";
    }
  }

  char *outval = NPN_StrDup(value.str().c_str());
  STRINGZ_TO_NPVARIANT(outval, *result);
  return true;
}

static const NPClass kInvokeDefaultClass = {
  NP_CLASS_STRUCT_VERSION,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  scriptableInvokeDefault,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr
};

bool
getInvokeDefaultObject(NPObject* npobj, const NPVariant* args,
                       uint32_t argCount, NPVariant* result)
{
  if (0 != argCount) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  NPObject* testObject =
    NPN_CreateObject(npp, const_cast<NPClass*>(&kInvokeDefaultClass));
  OBJECT_TO_NPVARIANT(testObject, *result);
  return true;
}

bool
callOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  if (id->callOnDestroy)
    return false;

  if (1 != argCount || !NPVARIANT_IS_OBJECT(args[0]))
    return false;

  id->callOnDestroy = NPVARIANT_TO_OBJECT(args[0]);
  NPN_RetainObject(id->callOnDestroy);

  return true;
}

// On Linux at least, a windowed plugin resize causes Flash Player to
// reconnect to the browser window.  This method simulates that.
bool
reinitWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount,
             NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  if (!id->hasWidget)
    return false;

  pluginWidgetInit(id, id->window.window);
  return true;
}

bool
propertyAndMethod(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                  NPVariant* result)
{
  INT32_TO_NPVARIANT(5, *result);
  return true;
}

// Returns top-level window activation state as indicated by Cocoa NPAPI's
// NPCocoaEventWindowFocusChanged events - 'true' if active, 'false' if not.
// Throws an exception if no events have been received and thus this state
// is unknown.
bool
getTopLevelWindowActivationState(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                                 NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  // Throw an exception for unknown state.
  if (id->topLevelWindowActivationState == ACTIVATION_STATE_UNKNOWN) {
    return false;
  }

  if (id->topLevelWindowActivationState == ACTIVATION_STATE_ACTIVATED) {
    BOOLEAN_TO_NPVARIANT(true, *result);
  } else if (id->topLevelWindowActivationState == ACTIVATION_STATE_DEACTIVATED) {
    BOOLEAN_TO_NPVARIANT(false, *result);
  }

  return true;
}

bool
getTopLevelWindowActivationEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                                      NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  INT32_TO_NPVARIANT(id->topLevelWindowActivationEventCount, *result);

  return true;
}

// Returns top-level window activation state as indicated by Cocoa NPAPI's
// NPCocoaEventFocusChanged events - 'true' if active, 'false' if not.
// Throws an exception if no events have been received and thus this state
// is unknown.
bool
getFocusState(NPObject* npobj, const NPVariant* args, uint32_t argCount,
              NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  // Throw an exception for unknown state.
  if (id->focusState == ACTIVATION_STATE_UNKNOWN) {
    return false;
  }

  if (id->focusState == ACTIVATION_STATE_ACTIVATED) {
    BOOLEAN_TO_NPVARIANT(true, *result);
  } else if (id->focusState == ACTIVATION_STATE_DEACTIVATED) {
    BOOLEAN_TO_NPVARIANT(false, *result);
  }

  return true;
}

bool
getFocusEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                   NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  INT32_TO_NPVARIANT(id->focusEventCount, *result);

  return true;
}

bool
getEventModel(NPObject* npobj, const NPVariant* args, uint32_t argCount,
              NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  INT32_TO_NPVARIANT(id->eventModel, *result);

  return true;
}

static bool
ReflectorHasMethod(NPObject* npobj, NPIdentifier name)
{
  return false;
}

static bool
ReflectorHasProperty(NPObject* npobj, NPIdentifier name)
{
  return true;
}

static bool
ReflectorGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
  if (NPN_IdentifierIsString(name)) {
    char* s = NPN_UTF8FromIdentifier(name);
    STRINGZ_TO_NPVARIANT(s, *result);
    return true;
  }

  INT32_TO_NPVARIANT(NPN_IntFromIdentifier(name), *result);
  return true;
}

static const NPClass kReflectorNPClass = {
  NP_CLASS_STRUCT_VERSION,
  nullptr,
  nullptr,
  nullptr,
  ReflectorHasMethod,
  nullptr,
  nullptr,
  ReflectorHasProperty,
  ReflectorGetProperty,
  nullptr,
  nullptr,
  nullptr,
  nullptr
};

bool
getReflector(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (0 != argCount)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  NPObject* reflector =
    NPN_CreateObject(npp,
		     const_cast<NPClass*>(&kReflectorNPClass)); // retains
  OBJECT_TO_NPVARIANT(reflector, *result);
  return true;
}

bool isVisible(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  BOOLEAN_TO_NPVARIANT(id->window.clipRect.top != 0 ||
		       id->window.clipRect.left != 0 ||
		       id->window.clipRect.bottom != 0 ||
		       id->window.clipRect.right != 0, *result);
  return true;
}

bool getWindowPosition(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  NPObject* window = nullptr;
  NPError err = NPN_GetValue(npp, NPNVWindowNPObject, &window);
  if (NPERR_NO_ERROR != err || !window)
    return false;

  NPIdentifier arrayID = NPN_GetStringIdentifier("Array");
  NPVariant arrayFunctionV;
  bool ok = NPN_GetProperty(npp, window, arrayID, &arrayFunctionV);

  NPN_ReleaseObject(window);

  if (!ok)
    return false;

  if (!NPVARIANT_IS_OBJECT(arrayFunctionV)) {
    NPN_ReleaseVariantValue(&arrayFunctionV);
    return false;
  }
  NPObject* arrayFunction = NPVARIANT_TO_OBJECT(arrayFunctionV);

  NPVariant elements[4];
  INT32_TO_NPVARIANT(id->window.x, elements[0]);
  INT32_TO_NPVARIANT(id->window.y, elements[1]);
  INT32_TO_NPVARIANT(id->window.width, elements[2]);
  INT32_TO_NPVARIANT(id->window.height, elements[3]);

  ok = NPN_InvokeDefault(npp, arrayFunction, elements, 4, result);

  NPN_ReleaseObject(arrayFunction);

  return ok;
}

bool constructObject(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount == 0 || !NPVARIANT_IS_OBJECT(args[0]))
    return false;

  NPObject* ctor = NPVARIANT_TO_OBJECT(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  return NPN_Construct(npp, ctor, args + 1, argCount - 1, result);
}

bool setSitesWithData(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1 || !NPVARIANT_IS_STRING(args[0]))
    return false;

  // Clear existing data.
  delete sSitesWithData;

  const NPString* str = &NPVARIANT_TO_STRING(args[0]);
  if (str->UTF8Length == 0)
    return true;

  // Parse the comma-delimited string into a vector.
  sSitesWithData = new list<siteData>;
  const char* iterator = str->UTF8Characters;
  const char* end = iterator + str->UTF8Length;
  while (1) {
    const char* next = strchr(iterator, ',');
    if (!next)
      next = end;

    // Parse out the three tokens into a siteData struct.
    const char* siteEnd = strchr(iterator, ':');
    *((char*) siteEnd) = '\0';
    const char* flagsEnd = strchr(siteEnd + 1, ':');
    *((char*) flagsEnd) = '\0';
    *((char*) next) = '\0';

    siteData data;
    data.site = string(iterator);
    data.flags = atoi(siteEnd + 1);
    data.age = atoi(flagsEnd + 1);

    sSitesWithData->push_back(data);

    if (next == end)
      break;

    iterator = next + 1;
  }

  return true;
}

bool setSitesWithDataCapabilities(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1 || !NPVARIANT_IS_BOOLEAN(args[0]))
    return false;

  sClearByAgeSupported = NPVARIANT_TO_BOOLEAN(args[0]);
  return true;
}

bool getLastKeyText(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                    NPVariant* result)
{
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  char *outval = NPN_StrDup(id->lastKeyText.c_str());
  STRINGZ_TO_NPVARIANT(outval, *result);
  return true;
}

bool getNPNVdocumentOrigin(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                           NPVariant* result)
{
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  char *origin = nullptr;
  NPError err = NPN_GetValue(npp, NPNVdocumentOrigin, &origin);
  if (err != NPERR_NO_ERROR) {
    return false;
  }

  STRINGZ_TO_NPVARIANT(origin, *result);
  return true;
}

bool getMouseUpEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->mouseUpEventCount, *result);
  return true;
}

bool queryContentsScaleFactor(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  double scaleFactor = 1.0;
#if defined(XP_MACOSX) || defined(XP_WIN)
  NPError err = NPN_GetValue(static_cast<TestNPObject*>(npobj)->npp,
                             NPNVcontentsScaleFactor, &scaleFactor);
  if (err != NPERR_NO_ERROR) {
    return false;
  }
#endif
  DOUBLE_TO_NPVARIANT(scaleFactor, *result);
  return true;
}

bool queryCSSZoomFactorSetValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  if (!npp) {
    return false;
  }
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  if (!id) {
    return false;
  }
  DOUBLE_TO_NPVARIANT(id->cssZoomFactor, *result);
  return true;
}

bool queryCSSZoomFactorGetValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  double zoomFactor = 1.0;
  NPError err = NPN_GetValue(static_cast<TestNPObject*>(npobj)->npp,
                             NPNVCSSZoomFactor, &zoomFactor);
  if (err != NPERR_NO_ERROR) {
    return false;
  }
  DOUBLE_TO_NPVARIANT(zoomFactor, *result);
  return true;
}

bool echoString(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1) {
    return false;
  }

  if (!NPVARIANT_IS_STRING(args[0])) {
    return false;
  }

  const NPString& arg = NPVARIANT_TO_STRING(args[0]);
  NPUTF8* buffer = static_cast<NPUTF8*>(NPN_MemAlloc(sizeof(NPUTF8) * arg.UTF8Length));
  if (!buffer) {
    return false;
  }

  std::copy(arg.UTF8Characters, arg.UTF8Characters + arg.UTF8Length, buffer);
  STRINGN_TO_NPVARIANT(buffer, arg.UTF8Length, *result);

  return true;
}

static bool
toggleAudioPlayback(NPObject* npobj, uint32_t argCount, bool playingAudio, NPVariant* result)
{
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->playingAudio = playingAudio;

  NPN_SetValue(npp, NPPVpluginIsPlayingAudio, (void*)playingAudio);

  VOID_TO_NPVARIANT(*result);
  return true;
}

static bool
startAudioPlayback(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  return toggleAudioPlayback(npobj, argCount, true, result);
}

static bool
stopAudioPlayback(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  return toggleAudioPlayback(npobj, argCount, false, result);
}

static bool
getAudioMuted(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0) {
    return false;
  }

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  BOOLEAN_TO_NPVARIANT(id->audioMuted, *result);
  return true;
}
