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
 *   Josh Aas <josh@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#ifndef nptest_h_
#define nptest_h_

#include "mozilla-config.h"

#include "npapi.h"
#include "npfunctions.h"
#include "npruntime.h"
#include <stdint.h>
#include <string>
#include <sstream>

typedef enum  {
  DM_DEFAULT,
  DM_SOLID_COLOR
} DrawMode;

typedef enum {
  FUNCTION_NONE,
  FUNCTION_NPP_GETURL,
  FUNCTION_NPP_GETURLNOTIFY,
  FUNCTION_NPP_POSTURL,
  FUNCTION_NPP_POSTURLNOTIFY,
  FUNCTION_NPP_NEWSTREAM,
  FUNCTION_NPP_WRITEREADY,
  FUNCTION_NPP_WRITE,
  FUNCTION_NPP_DESTROYSTREAM,
  FUNCTION_NPP_WRITE_RPC
} TestFunction;

typedef enum {
  ACTIVATION_STATE_UNKNOWN,
  ACTIVATION_STATE_ACTIVATED,
  ACTIVATION_STATE_DEACTIVATED
} ActivationState;

typedef struct FunctionTable {
  TestFunction funcId;
  const char* funcName;
} FunctionTable;

typedef enum {
  POSTMODE_FRAME,
  POSTMODE_STREAM
} PostMode;

typedef struct TestNPObject : NPObject {
  NPP npp;
  DrawMode drawMode;
  uint32_t drawColor; // 0xAARRGGBB
} TestNPObject;

typedef struct _PlatformData PlatformData;

typedef struct TestRange : NPByteRange {
  bool waiting;
} TestRange;

typedef struct InstanceData {
  NPP npp;
  NPWindow window;
  TestNPObject* scriptableObject;
  PlatformData* platformData;
  int32_t instanceCountWatchGeneration;
  bool lastReportedPrivateModeState;
  bool hasWidget;
  bool npnNewStream;
  bool throwOnNextInvoke;
  bool runScriptOnPaint;
  bool dontTouchElement;
  uint32_t timerID[2];
  bool timerTestResult;
  bool asyncCallbackResult;
  bool invalidateDuringPaint;
  bool slowPaint;
  bool playingAudio;
  bool audioMuted;
  int32_t winX;
  int32_t winY;
  int32_t lastMouseX;
  int32_t lastMouseY;
  int32_t widthAtLastPaint;
  int32_t paintCount;
  int32_t writeCount;
  int32_t writeReadyCount;
  int32_t asyncTestPhase;
  TestFunction testFunction;
  TestFunction functionToFail;
  NPError failureCode;
  NPObject* callOnDestroy;
  PostMode postMode;
  std::string testUrl;
  std::string frame;
  std::string timerTestScriptCallback;
  std::string asyncTestScriptCallback;
  std::ostringstream err;
  uint16_t streamMode;
  int32_t streamChunkSize;
  int32_t streamBufSize;
  int32_t fileBufSize;
  TestRange* testrange;
  void* streamBuf;
  void* fileBuf;
  bool crashOnDestroy;
  bool cleanupWidget;
  ActivationState topLevelWindowActivationState;
  int32_t topLevelWindowActivationEventCount;
  ActivationState focusState;
  int32_t focusEventCount;
  int32_t eventModel;
  bool closeStream;
  std::string lastKeyText;
  bool wantsAllStreams;
  int32_t mouseUpEventCount;
  int32_t bugMode;
  std::string javaCodebase;
} InstanceData;

void notifyDidPaint(InstanceData* instanceData);

#endif // nptest_h_
