/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2010, Mozilla Corporation
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
 *   Brad Lassey <blassey@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */
#include "nptest_platform.h"
#include "npapi.h"

struct _PlatformData {
};
 using namespace std;

bool
pluginSupportsWindowMode()
{
  return false;
}

bool
pluginSupportsWindowlessMode()
{
  return true;
}

NPError
pluginInstanceInit(InstanceData* instanceData)
{
  printf("NPERR_INCOMPATIBLE_VERSION_ERROR\n");
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
  NPN_MemFree(instanceData->platformData);
  instanceData->platformData = 0;
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  instanceData->window = *newWindow;
}

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
  // XXX nothing here yet since we don't support windowed plugins
}

static void
pluginDrawWindow(InstanceData* instanceData, void* event)
{
  return;
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
  return 0;
}

int32_t pluginGetEdge(InstanceData* instanceData, RectEdge edge)
{
  // XXX nothing here yet since we don't support windowed plugins
  return NPTEST_INT32_ERROR;
}

int32_t pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  // XXX nothing here yet since we don't support windowed plugins
  return NPTEST_INT32_ERROR;
}

int32_t pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  // XXX nothing here yet since we don't support windowed plugins
  return NPTEST_INT32_ERROR;
}

void pluginDoInternalConsistencyCheck(InstanceData* instanceData, string& error)
{
}
