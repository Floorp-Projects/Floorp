/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECORDEDEVENT_H_
#define MOZILLA_GFX_RECORDEDEVENT_H_

#include "2D.h"
#include <ostream>
#include <sstream>
#include <cstring>
#include <vector>

namespace mozilla {
namespace gfx {

struct PathOp;
class PathRecording;

const uint32_t kMagicInt = 0xc001feed;

// A change in major revision means a change in event binary format, causing
// loss of backwards compatibility. Old streams will not work in a player
// using a newer major revision. And new streams will not work in a player
// using an older major revision.
const uint16_t kMajorRevision = 10;
// A change in minor revision means additions of new events. New streams will
// not play in older players.
const uint16_t kMinorRevision = 0;

struct ReferencePtr
{
  ReferencePtr()
    : mLongPtr(0)
  {}

  MOZ_IMPLICIT ReferencePtr(const void* aLongPtr)
    : mLongPtr(uint64_t(aLongPtr))
  {}

  template <typename T>
  MOZ_IMPLICIT ReferencePtr(const RefPtr<T>& aPtr)
    : mLongPtr(uint64_t(aPtr.get()))
  {}

  ReferencePtr &operator =(const void* aLongPtr) {
    mLongPtr = uint64_t(aLongPtr);
    return *this;
  }

  template <typename T>
  ReferencePtr &operator =(const RefPtr<T>& aPtr) {
    mLongPtr = uint64_t(aPtr.get());
    return *this;
  }

  operator void*() const {
    return (void*)mLongPtr;
  }

  uint64_t mLongPtr;
};

struct RecordedFontDetails
{
  uint64_t fontDataKey;
  uint32_t size;
  uint32_t index;
};

// Used by the Azure drawing debugger (player2d)
inline std::string StringFromPtr(ReferencePtr aPtr)
{
  std::stringstream stream;
  stream << aPtr;
  return stream.str();
}

class Translator
{
public:
  virtual ~Translator() {}

  virtual DrawTarget *LookupDrawTarget(ReferencePtr aRefPtr) = 0;
  virtual Path *LookupPath(ReferencePtr aRefPtr) = 0;
  virtual SourceSurface *LookupSourceSurface(ReferencePtr aRefPtr) = 0;
  virtual FilterNode *LookupFilterNode(ReferencePtr aRefPtr) = 0;
  virtual GradientStops *LookupGradientStops(ReferencePtr aRefPtr) = 0;
  virtual ScaledFont *LookupScaledFont(ReferencePtr aRefPtr) = 0;
  virtual UnscaledFont* LookupUnscaledFont(ReferencePtr aRefPtr) = 0;
  virtual UnscaledFont* LookupUnscaledFontByIndex(size_t aIndex) { return nullptr; }
  virtual NativeFontResource *LookupNativeFontResource(uint64_t aKey) = 0;
  virtual already_AddRefed<SourceSurface> LookupExternalSurface(uint64_t aKey) { return nullptr; }
  virtual void AddDrawTarget(ReferencePtr aRefPtr, DrawTarget *aDT) = 0;
  virtual void RemoveDrawTarget(ReferencePtr aRefPtr) = 0;
  virtual void AddPath(ReferencePtr aRefPtr, Path *aPath) = 0;
  virtual void RemovePath(ReferencePtr aRefPtr) = 0;
  virtual void AddSourceSurface(ReferencePtr aRefPtr, SourceSurface *aPath) = 0;
  virtual void RemoveSourceSurface(ReferencePtr aRefPtr) = 0;
  virtual void AddFilterNode(mozilla::gfx::ReferencePtr aRefPtr, FilterNode *aSurface) = 0;
  virtual void RemoveFilterNode(mozilla::gfx::ReferencePtr aRefPtr) = 0;
  virtual void AddGradientStops(ReferencePtr aRefPtr, GradientStops *aPath) = 0;
  virtual void RemoveGradientStops(ReferencePtr aRefPtr) = 0;
  virtual void AddScaledFont(ReferencePtr aRefPtr, ScaledFont *aScaledFont) = 0;
  virtual void RemoveScaledFont(ReferencePtr aRefPtr) = 0;
  virtual void AddUnscaledFont(ReferencePtr aRefPtr, UnscaledFont* aUnscaledFont) = 0;
  virtual void RemoveUnscaledFont(ReferencePtr aRefPtr) = 0;
  virtual void AddNativeFontResource(uint64_t aKey,
                                     NativeFontResource *aNativeFontResource) = 0;

  virtual already_AddRefed<DrawTarget> CreateDrawTarget(ReferencePtr aRefPtr,
                                                        const IntSize &aSize,
                                                        SurfaceFormat aFormat);
  virtual DrawTarget *GetReferenceDrawTarget() = 0;
  virtual void* GetFontContext() { return nullptr; }
};

struct ColorPatternStorage
{
  Color mColor;
};

struct LinearGradientPatternStorage
{
  Point mBegin;
  Point mEnd;
  ReferencePtr mStops;
  Matrix mMatrix;
};

struct RadialGradientPatternStorage
{
  Point mCenter1;
  Point mCenter2;
  Float mRadius1;
  Float mRadius2;
  ReferencePtr mStops;
  Matrix mMatrix;
};

struct SurfacePatternStorage
{
  ExtendMode mExtend;
  SamplingFilter mSamplingFilter;
  ReferencePtr mSurface;
  Matrix mMatrix;
  IntRect mSamplingRect;
};

struct PatternStorage
{
  PatternType mType;
  union {
    char *mStorage;
    char mColor[sizeof(ColorPatternStorage)];
    char mLinear[sizeof(LinearGradientPatternStorage)];
    char mRadial[sizeof(RadialGradientPatternStorage)];
    char mSurface[sizeof(SurfacePatternStorage)];
  };
};


/* SizeCollector and MemWriter are used
 * in a pair to first collect the size of the
 * event that we're going to write and then
 * to write it without checking each individual
 * size. */
struct SizeCollector {
  SizeCollector() : mTotalSize(0) {}
    void write(const char*, size_t s) {
      mTotalSize += s;
    }
  size_t mTotalSize;
};

struct MemWriter {
  explicit MemWriter(char* aPtr) : mPtr(aPtr) {}
    void write(const char* aData, size_t aSize) {
       memcpy(mPtr, aData, aSize);
       mPtr += aSize;
    }
  char* mPtr;
};

struct MemStream {
  char *mData;
  size_t mLength;
  size_t mCapacity;
  void Resize(size_t aSize) {
    mLength = aSize;
    if (mLength > mCapacity) {
      mCapacity = mCapacity * 2;
      // check if the doubled capacity is enough
      // otherwise use double mLength
      if (mLength > mCapacity) {
        mCapacity = mLength * 2;
      }
      mData = (char*)realloc(mData, mCapacity);
    }
  }

  void write(const char* aData, size_t aSize) {
    Resize(mLength + aSize);
    memcpy(mData + mLength - aSize, aData, aSize);
  }

  MemStream() : mData(nullptr), mLength(0), mCapacity(0) {}
  ~MemStream() { free(mData); }
};

class EventStream {
public:
  virtual void write(const char* aData, size_t aSize) = 0;
  virtual void read(char* aOut, size_t aSize) = 0;
};

class RecordedEvent {
public:
  enum EventType {
    DRAWTARGETCREATION = 0,
    DRAWTARGETDESTRUCTION,
    FILLRECT,
    STROKERECT,
    STROKELINE,
    CLEARRECT,
    COPYSURFACE,
    SETTRANSFORM,
    PUSHCLIP,
    PUSHCLIPRECT,
    POPCLIP,
    FILL,
    FILLGLYPHS,
    MASK,
    STROKE,
    DRAWSURFACE,
    DRAWSURFACEWITHSHADOW,
    PATHCREATION,
    PATHDESTRUCTION,
    SOURCESURFACECREATION,
    SOURCESURFACEDESTRUCTION,
    GRADIENTSTOPSCREATION,
    GRADIENTSTOPSDESTRUCTION,
    SNAPSHOT,
    SCALEDFONTCREATION,
    SCALEDFONTCREATIONBYINDEX,
    SCALEDFONTDESTRUCTION,
    MASKSURFACE,
    FILTERNODECREATION,
    FILTERNODEDESTRUCTION,
    DRAWFILTER,
    FILTERNODESETATTRIBUTE,
    FILTERNODESETINPUT,
    CREATESIMILARDRAWTARGET,
    CREATECLIPPEDDRAWTARGET,
    FONTDATA,
    FONTDESC,
    PUSHLAYER,
    PUSHLAYERWITHBLEND,
    POPLAYER,
    UNSCALEDFONTCREATION,
    UNSCALEDFONTDESTRUCTION,
    INTOLUMINANCE,
    EXTERNALSURFACECREATION,
  };
  static const uint32_t kTotalEventTypes = RecordedEvent::FILTERNODESETINPUT + 1;

  virtual ~RecordedEvent() {}

  static std::string GetEventName(EventType aType);

  /**
   * Play back this event using the translator. Note that derived classes should
   * only return false when there is a fatal error, as it will probably mean the
   * translation will abort.
   * @param aTranslator Translator to be used for retrieving other referenced
   *                    objects and making playback decisions.
   * @return true unless a fatal problem has occurred and playback should abort.
   */
  virtual bool PlayEvent(Translator *aTranslator) const { return true; }

  virtual void RecordToStream(std::ostream& aStream) const = 0;
  virtual void RecordToStream(EventStream& aStream) const = 0;
  virtual void RecordToStream(MemStream& aStream) const = 0;

  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const { }

  template<class S>
  void RecordPatternData(S &aStream, const PatternStorage &aPatternStorage) const;
  template<class S>
  void ReadPatternData(S &aStream, PatternStorage &aPatternStorage) const;
  void StorePattern(PatternStorage &aDestination, const Pattern &aSource) const;
  template<class S>
  void RecordStrokeOptions(S &aStream, const StrokeOptions &aStrokeOptions) const;
  template<class S>
  void ReadStrokeOptions(S &aStream, StrokeOptions &aStrokeOptions);

  virtual std::string GetName() const = 0;

  virtual ReferencePtr GetObjectRef() const = 0;

  virtual ReferencePtr GetDestinedDT() { return nullptr; }

  void OutputSimplePatternInfo(const PatternStorage &aStorage, std::stringstream &aOutput) const;

  template<class S>
  static RecordedEvent *LoadEvent(S &aStream, EventType aType);
  static RecordedEvent *LoadEventFromStream(std::istream &aStream, EventType aType);
  static RecordedEvent *LoadEventFromStream(EventStream& aStream, EventType aType);

  // An alternative to LoadEvent that avoids a heap allocation for the event.
  // This accepts a callable `f' that will take a RecordedEvent* as a single parameter
  template<class S, class F>
  static bool DoWithEvent(S &aStream, EventType aType, F f);

  EventType GetType() const { return (EventType)mType; }
protected:
  friend class DrawEventRecorderPrivate;
  friend class DrawEventRecorderFile;
  friend class DrawEventRecorderMemory;
  static void RecordUnscaledFont(UnscaledFont *aUnscaledFont, std::ostream *aOutput);
  static void RecordUnscaledFont(UnscaledFont *aUnscaledFont, MemStream &aOutput);
  template<class S>
  static void RecordUnscaledFontImpl(UnscaledFont *aUnscaledFont, S &aOutput);

  MOZ_IMPLICIT RecordedEvent(int32_t aType) : mType(aType)
  {}

  int32_t mType;
  std::vector<Float> mDashPatternStorage;
};

} // namespace gfx
} // namespace mozilla

#endif
