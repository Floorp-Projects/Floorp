/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ClearKeyAsyncShutdown.h"
#include "gmp-task-utils.h"

ClearKeyAsyncShutdown::ClearKeyAsyncShutdown(GMPAsyncShutdownHost *aHostAPI)
  : mHost(aHostAPI)
{
  CK_LOGD("ClearKeyAsyncShutdown::ClearKeyAsyncShutdown");
  AddRef();
}

ClearKeyAsyncShutdown::~ClearKeyAsyncShutdown()
{
  CK_LOGD("ClearKeyAsyncShutdown::~ClearKeyAsyncShutdown");
}

void ShutdownTask(ClearKeyAsyncShutdown* aSelf, GMPAsyncShutdownHost* aHost)
{
  // Dumb implementation that just immediately reports completion.
  // Real GMPs should ensure they are properly shutdown.
  CK_LOGD("ClearKeyAsyncShutdown::BeginShutdown calling ShutdownComplete");
  aHost->ShutdownComplete();
  aSelf->Release();
}

void ClearKeyAsyncShutdown::BeginShutdown()
{
  CK_LOGD("ClearKeyAsyncShutdown::BeginShutdown dispatching asynchronous shutdown task");
  GetPlatform()->runonmainthread(WrapTaskNM(ShutdownTask, this, mHost));
}
