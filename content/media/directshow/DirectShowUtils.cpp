/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "dshow.h"
#include "Dmodshow.h"
#include "Wmcodecdsp.h"
#include "Dmoreg.h"
#include "DirectShowUtils.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"
#include "nsMemory.h"

namespace mozilla {

#if defined(PR_LOGGING)

// Create a table which maps GUIDs to a string representation of the GUID.
// This is useful for debugging purposes, for logging the GUIDs of media types.
// This is only available when logging is enabled, i.e. not in release builds.
struct GuidToName {
  const char* name;
  const GUID guid;
};

#pragma push_macro("OUR_GUID_ENTRY")
#undef OUR_GUID_ENTRY
#define OUR_GUID_ENTRY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
{ #name, {l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8} },

static const GuidToName GuidToNameTable[] = {
#include <uuids.h>
};

#pragma pop_macro("OUR_GUID_ENTRY")

const char*
GetDirectShowGuidName(const GUID& aGuid)
{
  size_t len = NS_ARRAY_LENGTH(GuidToNameTable);
  for (unsigned i = 0; i < len; i++) {
    if (IsEqualGUID(aGuid, GuidToNameTable[i].guid)) {
      return GuidToNameTable[i].name;
    }
  }
  return "Unknown";
}
#endif // PR_LOGGING

void
RemoveGraphFromRunningObjectTable(DWORD aRotRegister)
{
  nsRefPtr<IRunningObjectTable> runningObjectTable;
  if (SUCCEEDED(GetRunningObjectTable(0, getter_AddRefs(runningObjectTable)))) {
    runningObjectTable->Revoke(aRotRegister);
  }
}

HRESULT
AddGraphToRunningObjectTable(IUnknown *aUnkGraph, DWORD *aOutRotRegister)
{
  HRESULT hr;

  nsRefPtr<IMoniker> moniker;
  nsRefPtr<IRunningObjectTable> runningObjectTable;

  hr = GetRunningObjectTable(0, getter_AddRefs(runningObjectTable));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  const size_t STRING_LENGTH = 256;
  WCHAR wsz[STRING_LENGTH];

  StringCchPrintfW(wsz,
                   STRING_LENGTH,
                   L"FilterGraph %08x pid %08x",
                   (DWORD_PTR)aUnkGraph,
                   GetCurrentProcessId());

  hr = CreateItemMoniker(L"!", wsz, getter_AddRefs(moniker));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = runningObjectTable->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE,
                                    aUnkGraph,
                                    moniker,
                                    aOutRotRegister);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return S_OK;
}

const char*
GetGraphNotifyString(long evCode)
{
#define CASE(x) case x: return #x
  switch(evCode) {
    CASE(EC_ACTIVATE); // A video window is being activated or deactivated.
    CASE(EC_BANDWIDTHCHANGE); // Not supported.
    CASE(EC_BUFFERING_DATA); // The graph is buffering data, or has stopped buffering data.
    CASE(EC_BUILT); // Send by the Video Control when a graph has been built. Not forwarded to applications.
    CASE(EC_CLOCK_CHANGED); // The reference clock has changed.
    CASE(EC_CLOCK_UNSET); // The clock provider was disconnected.
    CASE(EC_CODECAPI_EVENT); // Sent by an encoder to signal an encoding event.
    CASE(EC_COMPLETE); // All data from a particular stream has been rendered.
    CASE(EC_CONTENTPROPERTY_CHANGED); // Not supported.
    CASE(EC_DEVICE_LOST); // A Plug and Play device was removed or has become available again.
    CASE(EC_DISPLAY_CHANGED); // The display mode has changed.
    CASE(EC_END_OF_SEGMENT); // The end of a segment has been reached.
    CASE(EC_EOS_SOON); // Not supported.
    CASE(EC_ERROR_STILLPLAYING); // An asynchronous command to run the graph has failed.
    CASE(EC_ERRORABORT); // An operation was aborted because of an error.
    CASE(EC_ERRORABORTEX); // An operation was aborted because of an error.
    CASE(EC_EXTDEVICE_MODE_CHANGE); // Not supported.
    CASE(EC_FILE_CLOSED); // The source file was closed because of an unexpected event.
    CASE(EC_FULLSCREEN_LOST); // The video renderer is switching out of full-screen mode.
    CASE(EC_GRAPH_CHANGED); // The filter graph has changed.
    CASE(EC_LENGTH_CHANGED); // The length of a source has changed.
    CASE(EC_LOADSTATUS); // Notifies the application of progress when opening a network file.
    CASE(EC_MARKER_HIT); // Not supported.
    CASE(EC_NEED_RESTART); // A filter is requesting that the graph be restarted.
    CASE(EC_NEW_PIN); // Not supported.
    CASE(EC_NOTIFY_WINDOW); // Notifies a filter of the video renderer's window.
    CASE(EC_OLE_EVENT); // A filter is passing a text string to the application.
    CASE(EC_OPENING_FILE); // The graph is opening a file, or has finished opening a file.
    CASE(EC_PALETTE_CHANGED); // The video palette has changed.
    CASE(EC_PAUSED); // A pause request has completed.
    CASE(EC_PLEASE_REOPEN); // The source file has changed.
    CASE(EC_PREPROCESS_COMPLETE); // Sent by the WM ASF Writer filter when it completes the pre-processing for multipass encoding.
    CASE(EC_PROCESSING_LATENCY); // Indicates the amount of time that a component is taking to process each sample.
    CASE(EC_QUALITY_CHANGE); // The graph is dropping samples, for quality control.
    //CASE(EC_RENDER_FINISHED); // Not supported.
    CASE(EC_REPAINT); // A video renderer requires a repaint.
    CASE(EC_SAMPLE_LATENCY); // Specifies how far behind schedule a component is for processing samples.
    //CASE(EC_SAMPLE_NEEDED); // Requests a new input sample from the Enhanced Video Renderer (EVR) filter.
    CASE(EC_SCRUB_TIME); // Specifies the time stamp for the most recent frame step.
    CASE(EC_SEGMENT_STARTED); // A new segment has started.
    CASE(EC_SHUTTING_DOWN); // The filter graph is shutting down, prior to being destroyed.
    CASE(EC_SNDDEV_IN_ERROR); // A device error has occurred in an audio capture filter.
    CASE(EC_SNDDEV_OUT_ERROR); // A device error has occurred in an audio renderer filter.
    CASE(EC_STARVATION); // A filter is not receiving enough data.
    CASE(EC_STATE_CHANGE); // The filter graph has changed state.
    CASE(EC_STATUS); // Contains two arbitrary status strings.
    CASE(EC_STEP_COMPLETE); // A filter performing frame stepping has stepped the specified number of frames.
    CASE(EC_STREAM_CONTROL_STARTED); // A stream-control start command has taken effect.
    CASE(EC_STREAM_CONTROL_STOPPED); // A stream-control stop command has taken effect.
    CASE(EC_STREAM_ERROR_STILLPLAYING); // An error has occurred in a stream. The stream is still playing.
    CASE(EC_STREAM_ERROR_STOPPED); // A stream has stopped because of an error.
    CASE(EC_TIMECODE_AVAILABLE); // Not supported.
    CASE(EC_UNBUILT); // Send by the Video Control when a graph has been torn down. Not forwarded to applications.
    CASE(EC_USERABORT); // The user has terminated playback.
    CASE(EC_VIDEO_SIZE_CHANGED); // The native video size has changed.
    CASE(EC_VIDEOFRAMEREADY); // A video frame is ready for display.
    CASE(EC_VMR_RECONNECTION_FAILED); // Sent by the VMR-7 and the VMR-9 when it was unable to accept a dynamic format change request from the upstream decoder.
    CASE(EC_VMR_RENDERDEVICE_SET); // Sent when the VMR has selected its rendering mechanism.
    CASE(EC_VMR_SURFACE_FLIPPED); // Sent when the VMR-7's allocator presenter has called the DirectDraw Flip method on the surface being presented.
    CASE(EC_WINDOW_DESTROYED); // The video renderer was destroyed or removed from the graph.
    CASE(EC_WMT_EVENT); // Sent by the WM ASF Reader filter when it reads ASF files protected by digital rights management (DRM).
    CASE(EC_WMT_INDEX_EVENT); // Sent when an application uses the WM ASF Writer to index Windows Media Video files.
    CASE(S_OK); // Success.
    CASE(VFW_S_AUDIO_NOT_RENDERED); // Partial success; the audio was not rendered.
    CASE(VFW_S_DUPLICATE_NAME); // Success; the Filter Graph Manager modified a filter name to avoid duplication.
    CASE(VFW_S_PARTIAL_RENDER); // Partial success; some of the streams in this movie are in an unsupported format.
    CASE(VFW_S_VIDEO_NOT_RENDERED); // Partial success; the video was not rendered.
    CASE(E_ABORT); // Operation aborted.
    CASE(E_OUTOFMEMORY); // Insufficient memory.
    CASE(E_POINTER); // NULL pointer argument.
    CASE(VFW_E_CANNOT_CONNECT); // No combination of intermediate filters could be found to make the connection.
    CASE(VFW_E_CANNOT_RENDER); // No combination of filters could be found to render the stream.
    CASE(VFW_E_NO_ACCEPTABLE_TYPES); // There is no common media type between these pins.
    CASE(VFW_E_NOT_IN_GRAPH);

    default:
      return "Unknown Code";
  };
#undef CASE
}

HRESULT
CreateAndAddFilter(IGraphBuilder* aGraph,
                   REFGUID aFilterClsId,
                   LPCWSTR aFilterName,
                   IBaseFilter **aOutFilter)
{
  NS_ENSURE_TRUE(aGraph, E_POINTER);
  NS_ENSURE_TRUE(aOutFilter, E_POINTER);
  HRESULT hr;

  nsRefPtr<IBaseFilter> filter;
  hr = CoCreateInstance(aFilterClsId,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_IBaseFilter,
                        getter_AddRefs(filter));
  if (FAILED(hr)) {
    // Object probably not available on this system.
    return hr;
  }

  hr = aGraph->AddFilter(filter, aFilterName);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  filter.forget(aOutFilter);

  return S_OK;
}

HRESULT
AddMP3DMOWrapperFilter(IGraphBuilder* aGraph,
                       IBaseFilter **aOutFilter)
{
  NS_ENSURE_TRUE(aGraph, E_POINTER);
  NS_ENSURE_TRUE(aOutFilter, E_POINTER);
  HRESULT hr;

  // Create the wrapper filter.
  nsRefPtr<IBaseFilter> filter;
  hr = CoCreateInstance(CLSID_DMOWrapperFilter,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_IBaseFilter,
                        getter_AddRefs(filter));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Query for IDMOWrapperFilter.
  nsRefPtr<IDMOWrapperFilter> dmoWrapper;
  hr = filter->QueryInterface(IID_IDMOWrapperFilter,
                              getter_AddRefs(dmoWrapper));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = dmoWrapper->Init(CLSID_CMP3DecMediaObject, DMOCATEGORY_AUDIO_DECODER);
  if (FAILED(hr)) {
    // Can't instantiate MP3 DMO. It doesn't exist on Windows XP, we're
    // probably hitting that. Don't log warning to console, this is an
    // expected error.
    return hr;
  }

  // Add the wrapper filter to graph.
  hr = aGraph->AddFilter(filter, L"MP3 Decoder DMO");
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  filter.forget(aOutFilter);

  return S_OK;
}

// Match a pin by pin direction and connection state.
HRESULT
MatchUnconnectedPin(IPin* aPin,
                    PIN_DIRECTION aPinDir,
                    bool *aOutMatches)
{
  NS_ENSURE_TRUE(aPin, E_POINTER);
  NS_ENSURE_TRUE(aOutMatches, E_POINTER);

  // Ensure the pin is unconnected.
  RefPtr<IPin> peer;
  HRESULT hr = aPin->ConnectedTo(byRef(peer));
  if (hr != VFW_E_NOT_CONNECTED) {
    *aOutMatches = false;
    return hr;
  }

  // Ensure the pin is of the specified direction.
  PIN_DIRECTION pinDir;
  hr = aPin->QueryDirection(&pinDir);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  *aOutMatches = (pinDir == aPinDir);
  return S_OK;
}

// Return the first unconnected input pin or output pin.
TemporaryRef<IPin>
GetUnconnectedPin(IBaseFilter* aFilter, PIN_DIRECTION aPinDir)
{
  RefPtr<IEnumPins> enumPins;

  HRESULT hr = aFilter->EnumPins(byRef(enumPins));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  // Test each pin to see if it matches the direction we're looking for.
  RefPtr<IPin> pin;
  while (S_OK == enumPins->Next(1, byRef(pin), NULL)) {
    bool matches = FALSE;
    if (SUCCEEDED(MatchUnconnectedPin(pin, aPinDir, &matches)) &&
        matches) {
      return pin;
    }
  }

  return nullptr;
}

HRESULT
ConnectFilters(IGraphBuilder* aGraph,
               IBaseFilter* aOutputFilter,
               IBaseFilter* aInputFilter)
{
  RefPtr<IPin> output = GetUnconnectedPin(aOutputFilter, PINDIR_OUTPUT);
  NS_ENSURE_TRUE(output, E_FAIL);

  RefPtr<IPin> input = GetUnconnectedPin(aInputFilter, PINDIR_INPUT);
  NS_ENSURE_TRUE(output, E_FAIL);

  return aGraph->Connect(output, input);
}

} // namespace mozilla
