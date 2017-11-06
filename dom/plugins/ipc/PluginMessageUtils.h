/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_PLUGINS_PLUGINMESSAGEUTILS_H
#define DOM_PLUGINS_PLUGINMESSAGEUTILS_H

#include "ipc/IPCMessageUtils.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"

#include "mozilla/ipc/CrossProcessMutex.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/UniquePtr.h"
#include "gfxipc/ShadowLayerUtils.h"

#include "npapi.h"
#include "npruntime.h"
#include "npfunctions.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include "nsExceptionHandler.h"
#include "nsHashKeys.h"

#ifdef XP_MACOSX
#include "PluginInterposeOSX.h"
#else
namespace mac_plugin_interposing { class NSCursorInfo { }; }
#endif
using mac_plugin_interposing::NSCursorInfo;

namespace mozilla {
namespace plugins {

using layers::SurfaceDescriptorX11;

enum ScriptableObjectType
{
  LocalObject,
  Proxy
};

mozilla::ipc::RacyInterruptPolicy
MediateRace(const mozilla::ipc::MessageChannel::MessageInfo& parent,
            const mozilla::ipc::MessageChannel::MessageInfo& child);

std::string
MungePluginDsoPath(const std::string& path);
std::string
UnmungePluginDsoPath(const std::string& munged);

extern mozilla::LogModule* GetPluginLog();

#if defined(_MSC_VER)
#define FULLFUNCTION __FUNCSIG__
#elif defined(__GNUC__)
#define FULLFUNCTION __PRETTY_FUNCTION__
#else
#define FULLFUNCTION __FUNCTION__
#endif

#define PLUGIN_LOG_DEBUG(args) MOZ_LOG(GetPluginLog(), mozilla::LogLevel::Debug, args)
#define PLUGIN_LOG_DEBUG_FUNCTION MOZ_LOG(GetPluginLog(), mozilla::LogLevel::Debug, ("%s", FULLFUNCTION))
#define PLUGIN_LOG_DEBUG_METHOD MOZ_LOG(GetPluginLog(), mozilla::LogLevel::Debug, ("%s [%p]", FULLFUNCTION, (void*) this))

/**
 * This is NPByteRange without the linked list.
 */
struct IPCByteRange
{
  int32_t offset;
  uint32_t length;
};

typedef nsTArray<IPCByteRange> IPCByteRanges;

typedef nsCString Buffer;

struct NPRemoteWindow
{
  NPRemoteWindow();
  uint64_t window;
  int32_t x;
  int32_t y;
  uint32_t width;
  uint32_t height;
  NPRect clipRect;
  NPWindowType type;
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
  VisualID visualID;
  Colormap colormap;
#endif /* XP_UNIX */
#if defined(XP_MACOSX) || defined(XP_WIN)
  double contentsScaleFactor;
#endif
};

// This struct is like NPAudioDeviceChangeDetails, only it uses a
// std::wstring instead of a const wchar_t* for the defaultDevice.
// This gives us the necessary memory-ownership semantics without
// requiring C++ objects in npapi.h.
struct NPAudioDeviceChangeDetailsIPC
{
  int32_t flow;
  int32_t role;
  std::wstring defaultDevice;
};

#ifdef XP_WIN
typedef HWND NativeWindowHandle;
#elif defined(MOZ_X11)
typedef XID NativeWindowHandle;
#elif defined(XP_DARWIN) || defined(ANDROID)
typedef intptr_t NativeWindowHandle; // never actually used, will always be 0
#else
#error Need NativeWindowHandle for this platform
#endif

#ifdef XP_WIN
typedef base::SharedMemoryHandle WindowsSharedMemoryHandle;
typedef HANDLE DXGISharedSurfaceHandle;
#else  // XP_WIN
typedef mozilla::null_t WindowsSharedMemoryHandle;
typedef mozilla::null_t DXGISharedSurfaceHandle;
#endif

// XXX maybe not the best place for these. better one?

#define VARSTR(v_)  case v_: return #v_
inline const char*
NPPVariableToString(NPPVariable aVar)
{
    switch (aVar) {
        VARSTR(NPPVpluginNameString);
        VARSTR(NPPVpluginDescriptionString);
        VARSTR(NPPVpluginWindowBool);
        VARSTR(NPPVpluginTransparentBool);
        VARSTR(NPPVjavaClass);
        VARSTR(NPPVpluginWindowSize);
        VARSTR(NPPVpluginTimerInterval);

        VARSTR(NPPVpluginScriptableInstance);
        VARSTR(NPPVpluginScriptableIID);

        VARSTR(NPPVjavascriptPushCallerBool);

        VARSTR(NPPVpluginKeepLibraryInMemory);
        VARSTR(NPPVpluginNeedsXEmbed);

        VARSTR(NPPVpluginScriptableNPObject);

        VARSTR(NPPVformValue);

        VARSTR(NPPVpluginUrlRequestsDisplayedBool);

        VARSTR(NPPVpluginWantsAllNetworkStreams);

#ifdef XP_MACOSX
        VARSTR(NPPVpluginDrawingModel);
        VARSTR(NPPVpluginEventModel);
#endif

#ifdef XP_WIN
        VARSTR(NPPVpluginRequiresAudioDeviceChanges);
#endif

    default: return "???";
    }
}

inline const char*
NPNVariableToString(NPNVariable aVar)
{
    switch(aVar) {
        VARSTR(NPNVxDisplay);
        VARSTR(NPNVxtAppContext);
        VARSTR(NPNVnetscapeWindow);
        VARSTR(NPNVjavascriptEnabledBool);
        VARSTR(NPNVasdEnabledBool);
        VARSTR(NPNVisOfflineBool);

        VARSTR(NPNVserviceManager);
        VARSTR(NPNVDOMElement);
        VARSTR(NPNVDOMWindow);
        VARSTR(NPNVToolkit);
        VARSTR(NPNVSupportsXEmbedBool);

        VARSTR(NPNVWindowNPObject);

        VARSTR(NPNVPluginElementNPObject);

        VARSTR(NPNVSupportsWindowless);

        VARSTR(NPNVprivateModeBool);
        VARSTR(NPNVdocumentOrigin);

#ifdef XP_WIN
        VARSTR(NPNVaudioDeviceChangeDetails);
#endif

    default: return "???";
    }
}
#undef VARSTR

inline bool IsPluginThread()
{
  MessageLoop* loop = MessageLoop::current();
  if (!loop)
      return false;
  return (loop->type() == MessageLoop::TYPE_UI);
}

inline void AssertPluginThread()
{
  MOZ_RELEASE_ASSERT(IsPluginThread(), "Should be on the plugin's main thread!");
}

#define ENSURE_PLUGIN_THREAD(retval) \
  PR_BEGIN_MACRO \
    if (!IsPluginThread()) { \
      NS_WARNING("Not running on the plugin's main thread!"); \
      return (retval); \
    } \
  PR_END_MACRO

#define ENSURE_PLUGIN_THREAD_VOID() \
  PR_BEGIN_MACRO \
    if (!IsPluginThread()) { \
      NS_WARNING("Not running on the plugin's main thread!"); \
      return; \
    } \
  PR_END_MACRO

void DeferNPObjectLastRelease(const NPNetscapeFuncs* f, NPObject* o);
void DeferNPVariantLastRelease(const NPNetscapeFuncs* f, NPVariant* v);

inline bool IsDrawingModelDirect(int16_t aModel)
{
    return aModel == NPDrawingModelAsyncBitmapSurface
#if defined(XP_WIN)
           || aModel == NPDrawingModelAsyncWindowsDXGISurface
#endif
           ;
}

// in NPAPI, char* == nullptr is sometimes meaningful.  the following is
// helper code for dealing with nullable nsCString's
inline nsCString
NullableString(const char* aString)
{
    if (!aString) {
        return VoidCString();
    }
    return nsCString(aString);
}

inline const char*
NullableStringGet(const nsCString& str)
{
  if (str.IsVoid())
    return nullptr;

  return str.get();
}

struct DeletingObjectEntry : public nsPtrHashKey<NPObject>
{
  explicit DeletingObjectEntry(const NPObject* key)
    : nsPtrHashKey<NPObject>(key)
    , mDeleted(false)
  { }

  bool mDeleted;
};

} /* namespace plugins */

} /* namespace mozilla */

namespace IPC {

template <>
struct ParamTraits<NPRect>
{
  typedef NPRect paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.top);
    WriteParam(aMsg, aParam.left);
    WriteParam(aMsg, aParam.bottom);
    WriteParam(aMsg, aParam.right);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    uint16_t top, left, bottom, right;
    if (ReadParam(aMsg, aIter, &top) &&
        ReadParam(aMsg, aIter, &left) &&
        ReadParam(aMsg, aIter, &bottom) &&
        ReadParam(aMsg, aIter, &right)) {
      aResult->top = top;
      aResult->left = left;
      aResult->bottom = bottom;
      aResult->right = right;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%u, %u, %u, %u]", aParam.top, aParam.left,
                              aParam.bottom, aParam.right));
  }
};

template <>
struct ParamTraits<NPWindowType> :
  public ContiguousEnumSerializerInclusive<NPWindowType,
                                           NPWindowType::NPWindowTypeWindow,
                                           NPWindowType::NPWindowTypeDrawable>
{};

template <>
struct ParamTraits<mozilla::plugins::NPRemoteWindow>
{
  typedef mozilla::plugins::NPRemoteWindow paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteUInt64(aParam.window);
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
    WriteParam(aMsg, aParam.clipRect);
    WriteParam(aMsg, aParam.type);
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    aMsg->WriteULong(aParam.visualID);
    aMsg->WriteULong(aParam.colormap);
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
    aMsg->WriteDouble(aParam.contentsScaleFactor);
#endif
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    uint64_t window;
    int32_t x, y;
    uint32_t width, height;
    NPRect clipRect;
    NPWindowType type;
    if (!(aMsg->ReadUInt64(aIter, &window) &&
          ReadParam(aMsg, aIter, &x) &&
          ReadParam(aMsg, aIter, &y) &&
          ReadParam(aMsg, aIter, &width) &&
          ReadParam(aMsg, aIter, &height) &&
          ReadParam(aMsg, aIter, &clipRect) &&
          ReadParam(aMsg, aIter, &type)))
      return false;

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    unsigned long visualID;
    unsigned long colormap;
    if (!(aMsg->ReadULong(aIter, &visualID) &&
          aMsg->ReadULong(aIter, &colormap)))
      return false;
#endif

#if defined(XP_MACOSX) || defined(XP_WIN)
    double contentsScaleFactor;
    if (!aMsg->ReadDouble(aIter, &contentsScaleFactor))
      return false;
#endif

    aResult->window = window;
    aResult->x = x;
    aResult->y = y;
    aResult->width = width;
    aResult->height = height;
    aResult->clipRect = clipRect;
    aResult->type = type;
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    aResult->visualID = visualID;
    aResult->colormap = colormap;
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
    aResult->contentsScaleFactor = contentsScaleFactor;
#endif
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%u, %d, %d, %u, %u, %d",
                              (unsigned long)aParam.window,
                              aParam.x, aParam.y, aParam.width,
                              aParam.height, (long)aParam.type));
  }
};

#ifdef XP_MACOSX
template <>
struct ParamTraits<NPNSString*>
{
  typedef NPNSString* paramType;

  // Empty string writes a length of 0 and no buffer.
  // We don't write a nullptr terminating character in buffers.
  static void Write(Message* aMsg, const paramType& aParam)
  {
    CFStringRef cfString = (CFStringRef)aParam;

    // Write true if we have a string, false represents nullptr.
    aMsg->WriteBool(!!cfString);
    if (!cfString) {
      return;
    }

    long length = ::CFStringGetLength(cfString);
    WriteParam(aMsg, length);
    if (length == 0) {
      return;
    }

    // Attempt to get characters without any allocation/conversion.
    if (::CFStringGetCharactersPtr(cfString)) {
      aMsg->WriteBytes(::CFStringGetCharactersPtr(cfString), length * sizeof(UniChar));
    } else {
      UniChar *buffer = (UniChar*)moz_xmalloc(length * sizeof(UniChar));
      ::CFStringGetCharacters(cfString, ::CFRangeMake(0, length), buffer);
      aMsg->WriteBytes(buffer, length * sizeof(UniChar));
      free(buffer);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool haveString = false;
    if (!aMsg->ReadBool(aIter, &haveString)) {
      return false;
    }
    if (!haveString) {
      *aResult = nullptr;
      return true;
    }

    long length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    // Avoid integer multiplication overflow.
    if (length > INT_MAX / static_cast<long>(sizeof(UniChar))) {
      return false;
    }

    auto chars = mozilla::MakeUnique<UniChar[]>(length);
    if (length != 0) {
      if (!aMsg->ReadBytesInto(aIter, chars.get(), length * sizeof(UniChar))) {
        return false;
      }
    }

    *aResult = (NPNSString*)::CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)chars.get(),
                                                      length * sizeof(UniChar),
                                                      kCFStringEncodingUTF16, false);
    if (!*aResult) {
      return false;
    }

    return true;
  }
};
#endif

#ifdef XP_MACOSX
template <>
struct ParamTraits<NSCursorInfo>
{
  typedef NSCursorInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    NSCursorInfo::Type type = aParam.GetType();

    aMsg->WriteInt(type);

    nsPoint hotSpot = aParam.GetHotSpot();
    WriteParam(aMsg, hotSpot.x);
    WriteParam(aMsg, hotSpot.y);

    uint32_t dataLength = aParam.GetCustomImageDataLength();
    WriteParam(aMsg, dataLength);
    if (dataLength == 0) {
      return;
    }

    uint8_t* buffer = (uint8_t*)moz_xmalloc(dataLength);
    memcpy(buffer, aParam.GetCustomImageData(), dataLength);
    aMsg->WriteBytes(buffer, dataLength);
    free(buffer);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    NSCursorInfo::Type type;
    if (!aMsg->ReadInt(aIter, (int*)&type)) {
      return false;
    }

    nscoord hotSpotX, hotSpotY;
    if (!ReadParam(aMsg, aIter, &hotSpotX) ||
        !ReadParam(aMsg, aIter, &hotSpotY)) {
      return false;
    }

    uint32_t dataLength;
    if (!ReadParam(aMsg, aIter, &dataLength)) {
      return false;
    }

    auto data = mozilla::MakeUnique<uint8_t[]>(dataLength);
    if (dataLength != 0) {
      if (!aMsg->ReadBytesInto(aIter, data.get(), dataLength)) {
        return false;
      }
    }

    aResult->SetType(type);
    aResult->SetHotSpot(nsPoint(hotSpotX, hotSpotY));
    aResult->SetCustomImageData(data.get(), dataLength);

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    const char* typeName = aParam.GetTypeName();
    nsPoint hotSpot = aParam.GetHotSpot();
    int hotSpotX, hotSpotY;
#ifdef NS_COORD_IS_FLOAT
    hotSpotX = rint(hotSpot.x);
    hotSpotY = rint(hotSpot.y);
#else
    hotSpotX = hotSpot.x;
    hotSpotY = hotSpot.y;
#endif
    uint32_t dataLength = aParam.GetCustomImageDataLength();
    uint8_t* data = aParam.GetCustomImageData();

    aLog->append(StringPrintf(L"[%s, (%i %i), %u, %p]",
                              typeName, hotSpotX, hotSpotY, dataLength, data));
  }
};
#else
template<>
struct ParamTraits<NSCursorInfo>
{
  typedef NSCursorInfo paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    MOZ_CRASH("NSCursorInfo isn't meaningful on this platform");
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    MOZ_CRASH("NSCursorInfo isn't meaningful on this platform");
    return false;
  }
};
#endif // #ifdef XP_MACOSX

template <>
struct ParamTraits<mozilla::plugins::IPCByteRange>
{
  typedef mozilla::plugins::IPCByteRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.offset);
    WriteParam(aMsg, aParam.length);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    paramType p;
    if (ReadParam(aMsg, aIter, &p.offset) &&
        ReadParam(aMsg, aIter, &p.length)) {
      *aResult = p;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<NPNVariable> :
  public ContiguousEnumSerializer<NPNVariable,
                                  NPNVariable::NPNVxDisplay,
                                  NPNVariable::NPNVLast>
{};

// The only accepted value is NPNURLVariable::NPNURLVProxy
template<>
struct ParamTraits<NPNURLVariable> :
  public ContiguousEnumSerializerInclusive<NPNURLVariable,
                                           NPNURLVariable::NPNURLVProxy,
                                           NPNURLVariable::NPNURLVProxy>
{};

template<>
struct ParamTraits<NPCoordinateSpace> :
  public ContiguousEnumSerializerInclusive<NPCoordinateSpace,
                                           NPCoordinateSpace::NPCoordinateSpacePlugin,
                                           NPCoordinateSpace::NPCoordinateSpaceFlippedScreen>
{};

template <>
struct ParamTraits<mozilla::plugins::NPAudioDeviceChangeDetailsIPC>
{
  typedef mozilla::plugins::NPAudioDeviceChangeDetailsIPC paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.flow);
    WriteParam(aMsg, aParam.role);
    WriteParam(aMsg, aParam.defaultDevice);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    int32_t flow, role;
    std::wstring defaultDevice;
    if (ReadParam(aMsg, aIter, &flow) &&
        ReadParam(aMsg, aIter, &role) &&
        ReadParam(aMsg, aIter, &defaultDevice)) {
      aResult->flow = flow;
      aResult->role = role;
      aResult->defaultDevice = defaultDevice;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%d, %d, %S]", aParam.flow, aParam.role,
                              aParam.defaultDevice.c_str()));
  }
};

} /* namespace IPC */


// Serializing NPEvents is completely platform-specific and can be rather
// intricate depending on the platform.  So for readability we split it
// into separate files and have the only macro crud live here.
//
// NB: these guards are based on those where struct NPEvent is defined
// in npapi.h.  They should be kept in sync.
#if defined(XP_MACOSX)
#  include "mozilla/plugins/NPEventOSX.h"
#elif defined(XP_WIN)
#  include "mozilla/plugins/NPEventWindows.h"
#elif defined(ANDROID)
#  include "mozilla/plugins/NPEventAndroid.h"
#elif defined(XP_UNIX)
#  include "mozilla/plugins/NPEventUnix.h"
#else
#  error Unsupported platform
#endif

#endif /* DOM_PLUGINS_PLUGINMESSAGEUTILS_H */
