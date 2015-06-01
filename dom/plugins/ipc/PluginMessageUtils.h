/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_PLUGINS_PLUGINMESSAGEUTILS_H
#define DOM_PLUGINS_PLUGINMESSAGEUTILS_H

#include "ipc/IPCMessageUtils.h"
#include "base/message_loop.h"

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/CrossProcessMutex.h"
#include "gfxipc/ShadowLayerUtils.h"

#include "npapi.h"
#include "npruntime.h"
#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include "nsHashKeys.h"
#ifdef MOZ_CRASHREPORTER
#  include "nsExceptionHandler.h"
#endif
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
MediateRace(const mozilla::ipc::MessageChannel::Message& parent,
            const mozilla::ipc::MessageChannel::Message& child);

std::string
MungePluginDsoPath(const std::string& path);
std::string
UnmungePluginDsoPath(const std::string& munged);

extern PRLogModuleInfo* GetPluginLog();

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
#if defined(XP_WIN)
  base::SharedMemoryHandle surfaceHandle;
#endif
#if defined(XP_MACOSX)
  double contentsScaleFactor;
#endif
};

#ifdef XP_WIN
typedef HWND NativeWindowHandle;
#elif defined(MOZ_X11)
typedef XID NativeWindowHandle;
#elif defined(XP_MACOSX) || defined(ANDROID) || defined(MOZ_WIDGET_QT)
typedef intptr_t NativeWindowHandle; // never actually used, will always be 0
#else
#error Need NativeWindowHandle for this platform
#endif

#ifdef XP_WIN
typedef base::SharedMemoryHandle WindowsSharedMemoryHandle;
typedef HANDLE DXGISharedSurfaceHandle;
#else
typedef mozilla::null_t WindowsSharedMemoryHandle;
typedef mozilla::null_t DXGISharedSurfaceHandle;
#endif

// XXX maybe not the best place for these. better one?

#define VARSTR(v_)  case v_: return #v_
inline const char* const
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
  NS_ASSERTION(IsPluginThread(), "Should be on the plugin's main thread!");
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

// in NPAPI, char* == nullptr is sometimes meaningful.  the following is
// helper code for dealing with nullable nsCString's
inline nsCString
NullableString(const char* aString)
{
    if (!aString) {
        nsCString str;
        str.SetIsVoid(true);
        return str;
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

#ifdef XP_WIN
// The private event used for double-pass widgetless plugin rendering.
UINT DoublePassRenderingEvent();
#endif

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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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
struct ParamTraits<NPWindowType>
{
  typedef NPWindowType paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteInt16(int16_t(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int16_t result;
    if (aMsg->ReadInt16(aIter, &result)) {
      *aResult = paramType(result);
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%d", int16_t(aParam)));
  }
};

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
#if defined(XP_WIN)
    WriteParam(aMsg, aParam.surfaceHandle);
#endif
#if defined(XP_MACOSX)
    aMsg->WriteDouble(aParam.contentsScaleFactor);
#endif
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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

#if defined(XP_WIN)
    base::SharedMemoryHandle surfaceHandle;
    if (!ReadParam(aMsg, aIter, &surfaceHandle))
      return false;
#endif

#if defined(XP_MACOSX)
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
#if defined(XP_WIN)
    aResult->surfaceHandle = surfaceHandle;
#endif
#if defined(XP_MACOSX)
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

template <>
struct ParamTraits<NPString>
{
  typedef NPString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.UTF8Length);
    aMsg->WriteBytes(aParam.UTF8Characters,
                     aParam.UTF8Length * sizeof(NPUTF8));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->UTF8Length)) {
      int byteCount = aResult->UTF8Length * sizeof(NPUTF8);
      if (!byteCount) {
        aResult->UTF8Characters = "\0";
        return true;
      }

      const char* messageBuffer = nullptr;
      nsAutoArrayPtr<char> newBuffer(new char[byteCount]);
      if (newBuffer && aMsg->ReadBytes(aIter, &messageBuffer, byteCount )) {
        memcpy((void*)messageBuffer, newBuffer.get(), byteCount);
        aResult->UTF8Characters = newBuffer.forget();
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%s", aParam.UTF8Characters));
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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

    UniChar* buffer = nullptr;
    if (length != 0) {
      if (!aMsg->ReadBytes(aIter, (const char**)&buffer, length * sizeof(UniChar)) ||
          !buffer) {
        return false;
      }
    }

    *aResult = (NPNSString*)::CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)buffer,
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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

    uint8_t* data = nullptr;
    if (dataLength != 0) {
      if (!aMsg->ReadBytes(aIter, (const char**)&data, dataLength) || !data) {
        return false;
      }
    }

    aResult->SetType(type);
    aResult->SetHotSpot(nsPoint(hotSpotX, hotSpotY));
    aResult->SetCustomImageData(data, dataLength);

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
    NS_RUNTIMEABORT("NSCursorInfo isn't meaningful on this platform");
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    NS_RUNTIMEABORT("NSCursorInfo isn't meaningful on this platform");
    return false;
  }
};
#endif // #ifdef XP_MACOSX

template <>
struct ParamTraits<NPVariant>
{
  typedef NPVariant paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    if (NPVARIANT_IS_VOID(aParam)) {
      aMsg->WriteInt(0);
      return;
    }

    if (NPVARIANT_IS_NULL(aParam)) {
      aMsg->WriteInt(1);
      return;
    }

    if (NPVARIANT_IS_BOOLEAN(aParam)) {
      aMsg->WriteInt(2);
      WriteParam(aMsg, NPVARIANT_TO_BOOLEAN(aParam));
      return;
    }

    if (NPVARIANT_IS_INT32(aParam)) {
      aMsg->WriteInt(3);
      WriteParam(aMsg, NPVARIANT_TO_INT32(aParam));
      return;
    }

    if (NPVARIANT_IS_DOUBLE(aParam)) {
      aMsg->WriteInt(4);
      WriteParam(aMsg, NPVARIANT_TO_DOUBLE(aParam));
      return;
    }

    if (NPVARIANT_IS_STRING(aParam)) {
      aMsg->WriteInt(5);
      WriteParam(aMsg, NPVARIANT_TO_STRING(aParam));
      return;
    }

    NS_ERROR("Unsupported type!");
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int type;
    if (!aMsg->ReadInt(aIter, &type)) {
      return false;
    }

    switch (type) {
      case 0:
        VOID_TO_NPVARIANT(*aResult);
        return true;

      case 1:
        NULL_TO_NPVARIANT(*aResult);
        return true;

      case 2: {
        bool value;
        if (ReadParam(aMsg, aIter, &value)) {
          BOOLEAN_TO_NPVARIANT(value, *aResult);
          return true;
        }
      } break;

      case 3: {
        int32_t value;
        if (ReadParam(aMsg, aIter, &value)) {
          INT32_TO_NPVARIANT(value, *aResult);
          return true;
        }
      } break;

      case 4: {
        double value;
        if (ReadParam(aMsg, aIter, &value)) {
          DOUBLE_TO_NPVARIANT(value, *aResult);
          return true;
        }
      } break;

      case 5: {
        NPString value;
        if (ReadParam(aMsg, aIter, &value)) {
          STRINGN_TO_NPVARIANT(value.UTF8Characters, value.UTF8Length,
                               *aResult);
          return true;
        }
      } break;

      default:
        NS_ERROR("Unsupported type!");
    }

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (NPVARIANT_IS_VOID(aParam)) {
      aLog->append(L"[void]");
      return;
    }

    if (NPVARIANT_IS_NULL(aParam)) {
      aLog->append(L"[null]");
      return;
    }

    if (NPVARIANT_IS_BOOLEAN(aParam)) {
      LogParam(NPVARIANT_TO_BOOLEAN(aParam), aLog);
      return;
    }

    if (NPVARIANT_IS_INT32(aParam)) {
      LogParam(NPVARIANT_TO_INT32(aParam), aLog);
      return;
    }

    if (NPVARIANT_IS_DOUBLE(aParam)) {
      LogParam(NPVARIANT_TO_DOUBLE(aParam), aLog);
      return;
    }

    if (NPVARIANT_IS_STRING(aParam)) {
      LogParam(NPVARIANT_TO_STRING(aParam), aLog);
      return;
    }

    NS_ERROR("Unsupported type!");
  }
};

template <>
struct ParamTraits<mozilla::plugins::IPCByteRange>
{
  typedef mozilla::plugins::IPCByteRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.offset);
    WriteParam(aMsg, aParam.length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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
struct ParamTraits<NPNVariable>
{
  typedef NPNVariable paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, int(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int intval;
    if (ReadParam(aMsg, aIter, &intval)) {
      *aResult = paramType(intval);
      return true;
    }
    return false;
  }
};

template<>
struct ParamTraits<NPNURLVariable>
{
  typedef NPNURLVariable paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, int(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int intval;
    if (ReadParam(aMsg, aIter, &intval)) {
      switch (intval) {
      case NPNURLVCookie:
      case NPNURLVProxy:
        *aResult = paramType(intval);
        return true;
      }
    }
    return false;
  }
};

  
template<>
struct ParamTraits<NPCoordinateSpace>
{
  typedef NPCoordinateSpace paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, int32_t(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int32_t intval;
    if (ReadParam(aMsg, aIter, &intval)) {
      switch (intval) {
      case NPCoordinateSpacePlugin:
      case NPCoordinateSpaceWindow:
      case NPCoordinateSpaceFlippedWindow:
      case NPCoordinateSpaceScreen:
      case NPCoordinateSpaceFlippedScreen:
        *aResult = paramType(intval);
        return true;
      }
    }
    return false;
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
