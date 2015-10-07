/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceFilter.h"
#include "MediaResource.h"
#include "mozilla/RefPtr.h"
#include "DirectShowUtils.h"
#include "MP3FrameParser.h"
#include "mozilla/Logging.h"
#include <algorithm>

using namespace mozilla::media;

namespace mozilla {

// Define to trace what's on...
//#define DEBUG_SOURCE_TRACE 1

#if defined (DEBUG_SOURCE_TRACE)
PRLogModuleInfo* GetDirectShowLog();
#define DIRECTSHOW_LOG(...) MOZ_LOG(GetDirectShowLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#define DIRECTSHOW_LOG(...)
#endif

static HRESULT
DoGetInterface(IUnknown* aUnknown, void** aInterface)
{
  if (!aInterface)
    return E_POINTER;
  *aInterface = aUnknown;
  aUnknown->AddRef();
  return S_OK;
}

// Stores details of IAsyncReader::Request().
class ReadRequest {
public:

  ReadRequest(IMediaSample* aSample,
              DWORD_PTR aDwUser,
              uint32_t aOffset,
              uint32_t aCount)
    : mSample(aSample),
      mDwUser(aDwUser),
      mOffset(aOffset),
      mCount(aCount)
  {
    MOZ_COUNT_CTOR(ReadRequest);
  }

  ~ReadRequest() {
    MOZ_COUNT_DTOR(ReadRequest);
  }

  RefPtr<IMediaSample> mSample;
  DWORD_PTR mDwUser;
  uint32_t mOffset;
  uint32_t mCount;
};

// A wrapper around media resource that presents only a partition of the
// underlying resource to the caller to use. The partition returned is from
// an offset to the end of stream, and this object deals with ensuring
// the offsets and lengths etc are translated from the reduced partition
// exposed to the caller, to the absolute offsets of the underlying stream.
class MediaResourcePartition {
public:
  MediaResourcePartition(MediaResource* aResource,
                         int64_t aDataStart)
    : mResource(aResource),
      mDataOffset(aDataStart)
  {}

  int64_t GetLength() {
    int64_t len = mResource.GetLength();
    if (len == -1) {
      return len;
    }
    return std::max<int64_t>(0, len - mDataOffset);
  }
  nsresult ReadAt(int64_t aOffset, char* aBuffer,
                  uint32_t aCount, uint32_t* aBytes)
  {
    return mResource.ReadAt(aOffset + mDataOffset,
                            aBuffer,
                            aCount,
                            aBytes);
  }
  int64_t GetCachedDataEnd() {
    int64_t tell = mResource.GetResource()->Tell();
    int64_t dataEnd =
      mResource.GetResource()->GetCachedDataEnd(tell) - mDataOffset;
    return dataEnd;
  }
private:
  // MediaResource from which we read data.
  MediaResourceIndex mResource;
  int64_t mDataOffset;
};


// Output pin for SourceFilter, which implements IAsyncReader, to
// allow downstream filters to pull/read data from it. Downstream pins
// register to read data using Request(), and asynchronously wait for the
// reads to complete using WaitForNext(). They may also synchronously read
// using SyncRead(). This class is a delegate (tear off) of
// SourceFilter.
//
// We can expose only a segment of the MediaResource to the filter graph.
// This is used to strip off the ID3v2 tags from the stream, as DirectShow
// has trouble parsing some headers.
//
// Implements:
//  * IAsyncReader
//  * IPin
//  * IQualityControl
//  * IUnknown
//
class DECLSPEC_UUID("18e5cfb2-1015-440c-a65c-e63853235894")
OutputPin : public IAsyncReader,
            public BasePin
{
public:

  OutputPin(MediaResource* aMediaResource,
            SourceFilter* aParent,
            CriticalSection& aFilterLock,
            int64_t aMP3DataStart);
  virtual ~OutputPin();

  // IUnknown
  // Defer to ref counting to BasePin, which defers to owning nsBaseFilter.
  STDMETHODIMP_(ULONG) AddRef() override { return BasePin::AddRef(); }
  STDMETHODIMP_(ULONG) Release() override { return BasePin::Release(); }
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;

  // BasePin Overrides.
  // Determines if the pin accepts a specific media type.
  HRESULT CheckMediaType(const MediaType* aMediaType) override;

  // Retrieves a preferred media type, by index value.
  HRESULT GetMediaType(int aPosition, MediaType* aMediaType) override;

  // Releases the pin from a connection.
  HRESULT BreakConnect(void) override;

  // Determines whether a pin connection is suitable.
  HRESULT CheckConnect(IPin* aPin) override;


  // IAsyncReader overrides

  // The RequestAllocator method requests an allocator during the
  // pin connection.
  STDMETHODIMP RequestAllocator(IMemAllocator* aPreferred,
                                ALLOCATOR_PROPERTIES* aProps,
                                IMemAllocator** aActual) override;

  // The Request method queues an asynchronous request for data. Downstream
  // will call WaitForNext() when they want to retrieve the result.
  STDMETHODIMP Request(IMediaSample* aSample, DWORD_PTR aUserData) override;

  // The WaitForNext method waits for the next pending read request
  // to complete. This method fails if the graph is flushing.
  // Defers to SyncRead/5.
  STDMETHODIMP WaitForNext(DWORD aTimeout,
                           IMediaSample** aSamples,
                           DWORD_PTR* aUserData) override;

  // The SyncReadAligned method performs a synchronous read. The method
  // blocks until the request is completed. Defers to SyncRead/5. This
  // method does not fail if the graph is flushing.
  STDMETHODIMP SyncReadAligned(IMediaSample* aSample) override;

  // The SyncRead method performs a synchronous read. The method blocks
  // until the request is completed. Defers to SyncRead/5. This
  // method does not fail if the graph is flushing.
  STDMETHODIMP SyncRead(LONGLONG aPosition, LONG aLength, BYTE* aBuffer) override;

  // The Length method retrieves the total length of the stream.
  STDMETHODIMP Length(LONGLONG* aTotal, LONGLONG* aAvailable) override;

  // IPin Overrides
  STDMETHODIMP BeginFlush(void) override;
  STDMETHODIMP EndFlush(void) override;

  uint32_t GetAndResetBytesConsumedCount();

private:

  // Protects thread-shared data/structures (mFlushCount, mPendingReads).
  // WaitForNext() also waits on this monitor
  CriticalSection& mPinLock;

  // Signal used with mPinLock to implement WaitForNext().
  Signal mSignal;

  // The filter that owns us. Weak reference, as we're a delegate (tear off).
  SourceFilter* mParentSource;

  MediaResourcePartition mResource;

  // Counter, inc'd in BeginFlush(), dec'd in EndFlush(). Calls to this can
  // come from multiple threads and can interleave, hence the counter.
  int32_t mFlushCount;

  // Number of bytes that have been read from the output pin since the last
  // time GetAndResetBytesConsumedCount() was called.
  uint32_t mBytesConsumed;

  // Deque of ReadRequest* for reads that are yet to be serviced.
  // nsReadRequest's are stored on the heap, popper must delete them.
  nsDeque mPendingReads;

  // Flags if the downstream pin has QI'd for IAsyncReader. We refuse
  // connection if they don't query, as it means they're assuming that we're
  // a push filter, and we're not.
  bool mQueriedForAsyncReader;

};

// For mingw __uuidof support
#ifdef __CRT_UUID_DECL
}
__CRT_UUID_DECL(mozilla::OutputPin, 0x18e5cfb2,0x1015,0x440c,0xa6,0x5c,0xe6,0x38,0x53,0x23,0x58,0x94);
namespace mozilla {
#endif

OutputPin::OutputPin(MediaResource* aResource,
                     SourceFilter* aParent,
                     CriticalSection& aFilterLock,
                     int64_t aMP3DataStart)
  : BasePin(static_cast<BaseFilter*>(aParent),
            &aFilterLock,
            L"MozillaOutputPin",
            PINDIR_OUTPUT),
    mPinLock(aFilterLock),
    mSignal(&mPinLock),
    mParentSource(aParent),
    mResource(aResource, aMP3DataStart),
    mFlushCount(0),
    mBytesConsumed(0),
    mQueriedForAsyncReader(false)
{
  MOZ_COUNT_CTOR(OutputPin);
  DIRECTSHOW_LOG("OutputPin::OutputPin()");
}

OutputPin::~OutputPin()
{
  MOZ_COUNT_DTOR(OutputPin);
  DIRECTSHOW_LOG("OutputPin::~OutputPin()");
}

HRESULT
OutputPin::BreakConnect()
{
  mQueriedForAsyncReader = false;
  return BasePin::BreakConnect();
}

STDMETHODIMP
OutputPin::QueryInterface(REFIID aIId, void** aInterface)
{
  if (aIId == IID_IAsyncReader) {
    mQueriedForAsyncReader = true;
    return DoGetInterface(static_cast<IAsyncReader*>(this), aInterface);
  }

  if (aIId == __uuidof(OutputPin)) {
    AddRef();
    *aInterface = this;
    return S_OK;
  }

  return BasePin::QueryInterface(aIId, aInterface);
}

HRESULT
OutputPin::CheckConnect(IPin* aPin)
{
  // Our connection is only suitable if the downstream pin knows
  // that we're asynchronous (i.e. it queried for IAsyncReader).
  return mQueriedForAsyncReader ? S_OK : S_FALSE;
}

HRESULT
OutputPin::CheckMediaType(const MediaType* aMediaType)
{
  const MediaType *myMediaType = mParentSource->GetMediaType();

  if (IsEqualGUID(aMediaType->majortype, myMediaType->majortype) &&
      IsEqualGUID(aMediaType->subtype, myMediaType->subtype) &&
      IsEqualGUID(aMediaType->formattype, myMediaType->formattype))
  {
    DIRECTSHOW_LOG("OutputPin::CheckMediaType() Match: major=%s minor=%s TC=%d FSS=%d SS=%u",
                   GetDirectShowGuidName(aMediaType->majortype),
                   GetDirectShowGuidName(aMediaType->subtype),
                   aMediaType->TemporalCompression(),
                   aMediaType->bFixedSizeSamples,
                   aMediaType->SampleSize());
    return S_OK;
  }

  DIRECTSHOW_LOG("OutputPin::CheckMediaType() Failed to match: major=%s minor=%s TC=%d FSS=%d SS=%u",
                 GetDirectShowGuidName(aMediaType->majortype),
                 GetDirectShowGuidName(aMediaType->subtype),
                 aMediaType->TemporalCompression(),
                 aMediaType->bFixedSizeSamples,
                 aMediaType->SampleSize());
  return S_FALSE;
}

HRESULT
OutputPin::GetMediaType(int aPosition, MediaType* aMediaType)
{
  if (!aMediaType)
    return E_POINTER;

  if (aPosition == 0) {
    aMediaType->Assign(mParentSource->GetMediaType());
    return S_OK;
  }
  return VFW_S_NO_MORE_ITEMS;
}

static inline bool
IsPowerOf2(int32_t x) {
  return ((-x & x) != x);
}

STDMETHODIMP
OutputPin::RequestAllocator(IMemAllocator* aPreferred,
                            ALLOCATOR_PROPERTIES* aProps,
                            IMemAllocator** aActual)
{
  // Require the downstream pin to suggest what they want...
  if (!aPreferred) return E_POINTER;
  if (!aProps) return E_POINTER;
  if (!aActual) return E_POINTER;

  // We only care about alignment - our allocator will reject anything
  // which isn't power-of-2 aligned, so  so try a 4-byte aligned allocator.
  ALLOCATOR_PROPERTIES props;
  memcpy(&props, aProps, sizeof(ALLOCATOR_PROPERTIES));
  if (aProps->cbAlign == 0 || IsPowerOf2(aProps->cbAlign)) {
    props.cbAlign = 4;
  }

  // Limit allocator's number of buffers. We know that the media will most
  // likely be bound by network speed, not by decoding speed. We also
  // store the incoming data in a Gecko stream, if we don't limit buffers
  // here we'll end up duplicating a lot of storage. We must have enough
  // space for audio key frames to fit in the first batch of buffers however,
  // else pausing may fail for some downstream decoders.
  if (props.cBuffers > BaseFilter::sMaxNumBuffers) {
    props.cBuffers = BaseFilter::sMaxNumBuffers;
  }

  // The allocator properties that are actually used. We don't store
  // this, we need it for SetProperties() below to succeed.
  ALLOCATOR_PROPERTIES actualProps;
  HRESULT hr;

  if (aPreferred) {
    // Play nice and prefer the downstream pin's preferred allocator.
    hr = aPreferred->SetProperties(&props, &actualProps);
    if (SUCCEEDED(hr)) {
      aPreferred->AddRef();
      *aActual = aPreferred;
      return S_OK;
    }
  }

  // Else downstream hasn't requested a specific allocator, so create one...

  // Just create a default allocator. It's highly unlikely that we'll use
  // this anyway, as most parsers insist on using their own allocators.
  RefPtr<IMemAllocator> allocator;
  hr = CoCreateInstance(CLSID_MemoryAllocator,
                        0,
                        CLSCTX_INPROC_SERVER,
                        IID_IMemAllocator,
                        getter_AddRefs(allocator));
  if(FAILED(hr) || (allocator == nullptr)) {
    NS_WARNING("Can't create our own DirectShow allocator.");
    return hr;
  }

  // See if we can make it suitable
  hr = allocator->SetProperties(&props, &actualProps);
  if (SUCCEEDED(hr)) {
    // We need to release our refcount on pAlloc, and addref
    // it to pass a refcount to the caller - this is a net nothing.
    allocator.forget(aActual);
    return S_OK;
  }

  NS_WARNING("Failed to pick an allocator");
  return hr;
}

STDMETHODIMP
OutputPin::Request(IMediaSample* aSample, DWORD_PTR aDwUser)
{
  if (!aSample) return E_FAIL;

  CriticalSectionAutoEnter lock(*mLock);
  NS_ASSERTION(!mFlushCount, "Request() while flushing");

  if (mFlushCount)
    return VFW_E_WRONG_STATE;

  REFERENCE_TIME refStart = 0, refEnd = 0;
  if (FAILED(aSample->GetTime(&refStart, &refEnd))) {
    NS_WARNING("Sample incorrectly timestamped");
    return VFW_E_SAMPLE_TIME_NOT_SET;
  }

  // Convert reference time to bytes.
  uint32_t start = (uint32_t)(refStart / 10000000);
  uint32_t end = (uint32_t)(refEnd / 10000000);

  uint32_t numBytes = end - start;

  ReadRequest* request = new ReadRequest(aSample,
                                         aDwUser,
                                         start,
                                         numBytes);
  // Memory for |request| is free when it's popped from the completed
  // reads list.

  // Push this onto the queue of reads to be serviced.
  mPendingReads.Push(request);

  // Notify any threads blocked in WaitForNext() which are waiting for mPendingReads
  // to become non-empty.
  mSignal.Notify();

  return S_OK;
}

STDMETHODIMP
OutputPin::WaitForNext(DWORD aTimeout,
                       IMediaSample** aOutSample,
                       DWORD_PTR* aOutDwUser)
{
  NS_ASSERTION(aTimeout == 0 || aTimeout == INFINITE,
               "Oops, we don't handle this!");

  *aOutSample = nullptr;
  *aOutDwUser = 0;

  LONGLONG offset = 0;
  LONG count = 0;
  BYTE* buf = nullptr;

  {
    CriticalSectionAutoEnter lock(*mLock);

    // Wait until there's a pending read to service.
    while (aTimeout && mPendingReads.GetSize() == 0 && !mFlushCount) {
      // Note: No need to guard against shutdown-during-wait here, as
      // typically the thread doing the pull will have already called
      // Request(), so we won't Wait() here anyway. SyncRead() will fail
      // on shutdown.
      mSignal.Wait();
    }

    nsAutoPtr<ReadRequest> request(reinterpret_cast<ReadRequest*>(mPendingReads.PopFront()));
    if (!request)
      return VFW_E_WRONG_STATE;

    *aOutSample = request->mSample;
    *aOutDwUser = request->mDwUser;

    offset = request->mOffset;
    count = request->mCount;
    buf = nullptr;
    request->mSample->GetPointer(&buf);
    NS_ASSERTION(buf != nullptr, "Invalid buffer!");

    if (mFlushCount) {
      return VFW_E_TIMEOUT;
    }
  }

  return SyncRead(offset, count, buf);
}

STDMETHODIMP
OutputPin::SyncReadAligned(IMediaSample* aSample)
{
  {
    // Ignore reads while flushing.
    CriticalSectionAutoEnter lock(*mLock);
    if (mFlushCount) {
      return S_FALSE;
    }
  }

  if (!aSample)
    return E_FAIL;

  REFERENCE_TIME lStart = 0, lEnd = 0;
  if (FAILED(aSample->GetTime(&lStart, &lEnd))) {
    NS_WARNING("Sample incorrectly timestamped");
    return VFW_E_SAMPLE_TIME_NOT_SET;
  }

  // Convert reference time to bytes.
  int32_t start = (int32_t)(lStart / 10000000);
  int32_t end = (int32_t)(lEnd / 10000000);

  int32_t numBytes = end - start;

  // If the range extends off the end of stream, truncate to the end of stream
  // as per IAsyncReader specificiation.
  int64_t streamLength = mResource.GetLength();
  if (streamLength != -1) {
    // We know the exact length of the stream, fail if the requested offset
    // is beyond it.
    if (start > streamLength) {
      return VFW_E_BADALIGN;
    }

    // If the end of the chunk to read is off the end of the stream,
    // truncate it to the end of the stream.
    if ((start + numBytes) > streamLength) {
      numBytes = (uint32_t)(streamLength - start);
    }
  }

  BYTE* buf=0;
  aSample->GetPointer(&buf);

  return SyncRead(start, numBytes, buf);
}

STDMETHODIMP
OutputPin::SyncRead(LONGLONG aPosition,
                    LONG aLength,
                    BYTE* aBuffer)
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(aPosition >= 0, E_FAIL);
  NS_ENSURE_TRUE(aLength > 0, E_FAIL);
  NS_ENSURE_TRUE(aBuffer, E_POINTER);

  DIRECTSHOW_LOG("OutputPin::SyncRead(%lld, %d)", aPosition, aLength);
  {
    // Ignore reads while flushing.
    CriticalSectionAutoEnter lock(*mLock);
    if (mFlushCount) {
      return S_FALSE;
    }
  }

  uint32_t totalBytesRead = 0;
  nsresult rv = mResource.ReadAt(aPosition,
                                 reinterpret_cast<char*>(aBuffer),
                                 aLength,
                                 &totalBytesRead);
  if (NS_FAILED(rv)) {
    return E_FAIL;
  }
  if (totalBytesRead > 0) {
    CriticalSectionAutoEnter lock(*mLock);
    mBytesConsumed += totalBytesRead;
  }
  return (totalBytesRead == aLength) ? S_OK : S_FALSE;
}

STDMETHODIMP
OutputPin::Length(LONGLONG* aTotal, LONGLONG* aAvailable)
{
  HRESULT hr = S_OK;
  int64_t length = mResource.GetLength();
  if (length == -1) {
    hr = VFW_S_ESTIMATED;
    // Don't have a length. Just lie, it seems to work...
    *aTotal = INT32_MAX;
  } else {
    *aTotal = length;
  }
  if (aAvailable) {
    *aAvailable = mResource.GetCachedDataEnd();
  }

  DIRECTSHOW_LOG("OutputPin::Length() len=%lld avail=%lld", *aTotal, *aAvailable);

  return hr;
}

STDMETHODIMP
OutputPin::BeginFlush()
{
  CriticalSectionAutoEnter lock(*mLock);
  mFlushCount++;
  mSignal.Notify();
  return S_OK;
}

STDMETHODIMP
OutputPin::EndFlush(void)
{
  CriticalSectionAutoEnter lock(*mLock);
  mFlushCount--;
  return S_OK;
}

uint32_t
OutputPin::GetAndResetBytesConsumedCount()
{
  CriticalSectionAutoEnter lock(*mLock);
  uint32_t bytesConsumed = mBytesConsumed;
  mBytesConsumed = 0;
  return bytesConsumed;
}

SourceFilter::SourceFilter(const GUID& aMajorType,
                                               const GUID& aSubType)
  : BaseFilter(L"MozillaDirectShowSource", __uuidof(SourceFilter))
{
  MOZ_COUNT_CTOR(SourceFilter);
  mMediaType.majortype = aMajorType;
  mMediaType.subtype = aSubType;

  DIRECTSHOW_LOG("SourceFilter Constructor(%s, %s)",
                 GetDirectShowGuidName(aMajorType),
                 GetDirectShowGuidName(aSubType));
}

SourceFilter::~SourceFilter()
{
  MOZ_COUNT_DTOR(SourceFilter);
  DIRECTSHOW_LOG("SourceFilter Destructor()");
}

BasePin*
SourceFilter::GetPin(int n)
{
  if (n == 0) {
    NS_ASSERTION(mOutputPin != 0, "GetPin with no pin!");
    return static_cast<BasePin*>(mOutputPin);
  } else {
    return nullptr;
  }
}

// Get's the media type we're supplying.
const MediaType*
SourceFilter::GetMediaType() const
{
  return &mMediaType;
}

nsresult
SourceFilter::Init(MediaResource* aResource, int64_t aMP3Offset)
{
  DIRECTSHOW_LOG("SourceFilter::Init()");

  mOutputPin = new OutputPin(aResource,
                             this,
                             mLock,
                             aMP3Offset);
  NS_ENSURE_TRUE(mOutputPin != nullptr, NS_ERROR_FAILURE);

  return NS_OK;
}

uint32_t
SourceFilter::GetAndResetBytesConsumedCount()
{
  return mOutputPin->GetAndResetBytesConsumedCount();
}


} // namespace mozilla
