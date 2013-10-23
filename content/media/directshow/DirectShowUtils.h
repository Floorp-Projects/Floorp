/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _DirectShowUtils_h_
#define _DirectShowUtils_h_

#include <stdint.h>
#include "dshow.h"
#include "DShowTools.h"
#include "prlog.h"

namespace mozilla {

// Win32 "Event" wrapper. Must be paired with a CriticalSection to create a
// Java-style "monitor".
class Signal {
public:

  Signal(CriticalSection* aLock)
    : mLock(aLock)
  {
    CriticalSectionAutoEnter lock(*mLock);
    mEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  }

  ~Signal() {
    CriticalSectionAutoEnter lock(*mLock);
    CloseHandle(mEvent);
  }

  // Lock must be held.
  void Notify() {
    SetEvent(mEvent);
  }

  // Lock must be held. Check the wait condition before waiting!
  HRESULT Wait() {
    mLock->Leave();
    DWORD result = WaitForSingleObject(mEvent, INFINITE);
    mLock->Enter();
    return result == WAIT_OBJECT_0 ? S_OK : E_FAIL;
  }

private:
  CriticalSection* mLock;
  HANDLE mEvent;
};

HRESULT
AddGraphToRunningObjectTable(IUnknown *aUnkGraph, DWORD *aOutRotRegister);

void
RemoveGraphFromRunningObjectTable(DWORD aRotRegister);

const char*
GetGraphNotifyString(long evCode);

// Creates a filter and adds it to a graph.
HRESULT
CreateAndAddFilter(IGraphBuilder* aGraph,
                   REFGUID aFilterClsId,
                   LPCWSTR aFilterName,
                   IBaseFilter **aOutFilter);

HRESULT
AddMP3DMOWrapperFilter(IGraphBuilder* aGraph,
                       IBaseFilter **aOutFilter);

// Connects the output pin on aOutputFilter to an input pin on
// aInputFilter, in aGraph.
HRESULT
ConnectFilters(IGraphBuilder* aGraph,
               IBaseFilter* aOutputFilter,
               IBaseFilter* aInputFilter);

HRESULT
MatchUnconnectedPin(IPin* aPin,
                    PIN_DIRECTION aPinDir,
                    bool *aOutMatches);

// Converts from microseconds to DirectShow "Reference Time"
// (hundreds of nanoseconds).
inline int64_t
UsecsToRefTime(const int64_t aUsecs)
{
  return aUsecs * 10;
}

// Converts from DirectShow "Reference Time" (hundreds of nanoseconds)
// to microseconds.
inline int64_t
RefTimeToUsecs(const int64_t hRefTime)
{
  return hRefTime / 10;
}

// Converts from DirectShow "Reference Time" (hundreds of nanoseconds)
// to seconds.
inline double
RefTimeToSeconds(const REFERENCE_TIME aRefTime)
{
  return double(aRefTime) / 10000000;
}


#if defined(PR_LOGGING)
const char*
GetDirectShowGuidName(const GUID& aGuid);
#endif

} // namespace mozilla

#endif
