/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPService.h"
#include "GMPTestMonitor.h"
#include "gmp-api/gmp-video-host.h"
#include "gtest/gtest.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"

#define GMP_DIR_NAME NS_LITERAL_STRING("gmp-fake")
#define GMP_OLD_VERSION NS_LITERAL_STRING("1.0")
#define GMP_NEW_VERSION NS_LITERAL_STRING("1.1")

#define GMP_DELETED_TOPIC "gmp-directory-deleted"

#define EXPECT_OK(X) EXPECT_TRUE(NS_SUCCEEDED(X))

class GMPRemoveTest : public nsIObserver
                    , public GMPVideoDecoderCallbackProxy
{
public:
  GMPRemoveTest();

  NS_DECL_THREADSAFE_ISUPPORTS

  // Called when a GMP plugin directory has been successfully deleted.
  // |aData| will contain the directory path.
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override;

  // Create a new GMP plugin directory that we can trash and add it to the GMP
  // service. Remove the original plugin directory. Original plugin directory
  // gets re-added at destruction.
  void Setup();

  bool CreateVideoDecoder(nsCString aNodeId = EmptyCString());
  void CloseVideoDecoder();

  void DeletePluginDirectory(bool aCanDefer);

  // Decode a dummy frame.
  GMPErr Decode();

  // Wait until TestMonitor has been signaled.
  void Wait();

  // Did we get a Terminated() callback from the plugin?
  bool IsTerminated();

  // From GMPVideoDecoderCallbackProxy
  // Set mDecodeResult; unblock TestMonitor.
  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) override;
  virtual void Error(GMPErr aError) override;

  // From GMPVideoDecoderCallbackProxy
  // We expect this to be called when a plugin has been forcibly closed.
  virtual void Terminated() override;

  // Ignored GMPVideoDecoderCallbackProxy members
  virtual void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override {}
  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) override {}
  virtual void InputDataExhausted() override {}
  virtual void DrainComplete() override {}
  virtual void ResetComplete() override {}

private:
  virtual ~GMPRemoveTest();

  void gmp_Decode();
  void gmp_GetVideoDecoder(nsCString aNodeId,
                           GMPVideoDecoderProxy** aOutDecoder,
                           GMPVideoHost** aOutHost);
  void GeneratePlugin();

  GMPTestMonitor mTestMonitor;
  nsCOMPtr<nsIThread> mGMPThread;

  bool mIsTerminated;

  // Path to the cloned GMP we have created.
  nsString mTmpPath;
  nsCOMPtr<nsIFile> mTmpDir;

  // Path to the original GMP. Store so that we can re-add it after we're done
  // testing.
  nsString mOriginalPath;

  GMPVideoDecoderProxy* mDecoder;
  GMPVideoHost* mHost;
  GMPErr mDecodeResult;
};

/*
 * Simple test that the plugin is deleted when forcibly removed and deleted.
 */
TEST(GeckoMediaPlugins, RemoveAndDeleteForcedSimple)
{
  nsRefPtr<GMPRemoveTest> test(new GMPRemoveTest());

  test->Setup();
  test->DeletePluginDirectory(false /* force immediate */);
  test->Wait();
}

/*
 * Simple test that the plugin is deleted when deferred deletion is allowed.
 */
TEST(GeckoMediaPlugins, RemoveAndDeleteDeferredSimple)
{
  nsRefPtr<GMPRemoveTest> test(new GMPRemoveTest());

  test->Setup();
  test->DeletePluginDirectory(true /* can defer */);
  test->Wait();
}

/*
 * Test that the plugin is unavailable immediately after a forced
 * RemoveAndDelete, and that the plugin is deleted afterwards.
 */
TEST(GeckoMediaPlugins, RemoveAndDeleteForcedInUse)
{
  nsRefPtr<GMPRemoveTest> test(new GMPRemoveTest());

  test->Setup();
  EXPECT_TRUE(test->CreateVideoDecoder(NS_LITERAL_CSTRING("thisOrigin")));

  // Test that we can decode a frame.
  GMPErr err = test->Decode();
  EXPECT_EQ(err, GMPNoErr);

  test->DeletePluginDirectory(false /* force immediate */);
  test->Wait();

  // Test that the VideoDecoder is no longer available.
  EXPECT_FALSE(test->CreateVideoDecoder(NS_LITERAL_CSTRING("thisOrigin")));

  // Test that we were notified of the plugin's destruction.
  EXPECT_TRUE(test->IsTerminated());
}

/*
 * Test that the plugin is still usable after a deferred RemoveAndDelete, and
 * that the plugin is deleted afterwards.
 */
TEST(GeckoMediaPlugins, RemoveAndDeleteDeferredInUse)
{
  nsRefPtr<GMPRemoveTest> test(new GMPRemoveTest());

  test->Setup();
  EXPECT_TRUE(test->CreateVideoDecoder(NS_LITERAL_CSTRING("thisOrigin")));

  // Make sure decoding works before we do anything.
  GMPErr err = test->Decode();
  EXPECT_EQ(err, GMPNoErr);

  test->DeletePluginDirectory(true /* can defer */);

  // Test that decoding still works.
  err = test->Decode();
  EXPECT_EQ(err, GMPNoErr);

  // Test that other origins are not able to fetch the video decoder.
  EXPECT_FALSE(test->CreateVideoDecoder(NS_LITERAL_CSTRING("otherOrigin")));

  // Test that this origin is still able to fetch the video decoder.
  EXPECT_TRUE(test->CreateVideoDecoder(NS_LITERAL_CSTRING("thisOrigin")));

  test->CloseVideoDecoder();
  test->Wait();
}

static StaticRefPtr<GeckoMediaPluginService> gService;
static StaticRefPtr<GeckoMediaPluginServiceParent> gServiceParent;

static GeckoMediaPluginService*
GetService()
{
  if (!gService) {
    nsRefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    gService = service;
  }

  return gService.get();
}

static GeckoMediaPluginServiceParent*
GetServiceParent()
{
  if (!gServiceParent) {
    nsRefPtr<GeckoMediaPluginServiceParent> parent =
      GeckoMediaPluginServiceParent::GetSingleton();
    gServiceParent = parent;
  }

  return gServiceParent.get();
}

NS_IMPL_ISUPPORTS(GMPRemoveTest, nsIObserver)

GMPRemoveTest::GMPRemoveTest()
  : mIsTerminated(false)
  , mDecoder(nullptr)
  , mHost(nullptr)
{
}

GMPRemoveTest::~GMPRemoveTest()
{
  bool exists;
  EXPECT_TRUE(NS_SUCCEEDED(mTmpDir->Exists(&exists)) && !exists);

  EXPECT_OK(GetServiceParent()->AddPluginDirectory(mOriginalPath));
}

void
GMPRemoveTest::Setup()
{
  GeneratePlugin();
  EXPECT_OK(GetServiceParent()->RemovePluginDirectory(mOriginalPath));

  GetServiceParent()->AddPluginDirectory(mTmpPath);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, GMP_DELETED_TOPIC, false /* strong ref */);

  GetService()->GetThread(getter_AddRefs(mGMPThread));
}

bool
GMPRemoveTest::CreateVideoDecoder(nsCString aNodeId)
{
  GMPVideoHost* host;
  GMPVideoDecoderProxy* decoder = nullptr;

  mGMPThread->Dispatch(
    NS_NewRunnableMethodWithArgs<nsCString, GMPVideoDecoderProxy**, GMPVideoHost**>(
      this, &GMPRemoveTest::gmp_GetVideoDecoder, aNodeId, &decoder, &host),
    NS_DISPATCH_NORMAL);

  mTestMonitor.AwaitFinished();

  if (!decoder) {
    return false;
  }

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));
  codec.mGMPApiVersion = 33;

  nsTArray<uint8_t> empty;
  mGMPThread->Dispatch(
    NS_NewNonOwningRunnableMethodWithArgs<const GMPVideoCodec&, const nsTArray<uint8_t>&, GMPVideoDecoderCallbackProxy*, int32_t>(
      decoder, &GMPVideoDecoderProxy::InitDecode,
      codec, empty, this, 1 /* core count */),
    NS_DISPATCH_SYNC);

  if (mDecoder) {
    CloseVideoDecoder();
  }

  mDecoder = decoder;
  mHost = host;

  return true;
}

void
GMPRemoveTest::gmp_GetVideoDecoder(nsCString aNodeId,
                                   GMPVideoDecoderProxy** aOutDecoder,
                                   GMPVideoHost** aOutHost)
{
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));

  class Callback : public GetGMPVideoDecoderCallback
  {
  public:
    Callback(GMPTestMonitor* aMonitor, GMPVideoDecoderProxy** aDecoder, GMPVideoHost** aHost)
      : mMonitor(aMonitor), mDecoder(aDecoder), mHost(aHost) { }
    virtual void Done(GMPVideoDecoderProxy* aDecoder, GMPVideoHost* aHost) override {
      *mDecoder = aDecoder;
      *mHost = aHost;
      mMonitor->SetFinished();
    }
  private:
    GMPTestMonitor* mMonitor;
    GMPVideoDecoderProxy** mDecoder;
    GMPVideoHost** mHost;
  };

  UniquePtr<GetGMPVideoDecoderCallback>
    cb(new Callback(&mTestMonitor, aOutDecoder, aOutHost));

  if (NS_FAILED(GetService()->GetGMPVideoDecoder(&tags, aNodeId, Move(cb)))) {
    mTestMonitor.SetFinished();
  }
}

void
GMPRemoveTest::CloseVideoDecoder()
{
  mGMPThread->Dispatch(
    NS_NewNonOwningRunnableMethod(mDecoder, &GMPVideoDecoderProxy::Close),
    NS_DISPATCH_SYNC);

  mDecoder = nullptr;
  mHost = nullptr;
}

void
GMPRemoveTest::DeletePluginDirectory(bool aCanDefer)
{
  GetServiceParent()->RemoveAndDeletePluginDirectory(mTmpPath, aCanDefer);
}

GMPErr
GMPRemoveTest::Decode()
{
  mGMPThread->Dispatch(
    NS_NewNonOwningRunnableMethod(this, &GMPRemoveTest::gmp_Decode),
    NS_DISPATCH_NORMAL);

  mTestMonitor.AwaitFinished();
  return mDecodeResult;
}

void
GMPRemoveTest::gmp_Decode()
{
  // from gmp-fake.cpp
  struct EncodedFrame {
    uint32_t length_;
    uint8_t h264_compat_;
    uint32_t magic_;
    uint32_t width_;
    uint32_t height_;
    uint8_t y_;
    uint8_t u_;
    uint8_t v_;
    uint32_t timestamp_;
  };

  GMPVideoFrame* absFrame;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &absFrame);
  EXPECT_EQ(err, GMPNoErr);

  GMPUnique<GMPVideoEncodedFrame>::Ptr
    frame(static_cast<GMPVideoEncodedFrame*>(absFrame));
  err = frame->CreateEmptyFrame(sizeof(EncodedFrame) /* size */);
  EXPECT_EQ(err, GMPNoErr);

  EncodedFrame* frameData = reinterpret_cast<EncodedFrame*>(frame->Buffer());
  frameData->magic_ = 0x4652414d;
  frameData->width_ = frameData->height_ = 16;

  nsTArray<uint8_t> empty;
  nsresult rv = mDecoder->Decode(Move(frame), false /* aMissingFrames */, empty);
  EXPECT_OK(rv);
}

void
GMPRemoveTest::Wait()
{
  mTestMonitor.AwaitFinished();
}

bool
GMPRemoveTest::IsTerminated()
{
  return mIsTerminated;
}

// nsIObserver
NS_IMETHODIMP
GMPRemoveTest::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData)
{
  EXPECT_TRUE(!strcmp(GMP_DELETED_TOPIC, aTopic));

  nsString data(aData);
  if (mTmpPath.Equals(data)) {
    mTestMonitor.SetFinished();
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, GMP_DELETED_TOPIC);
  }

  return NS_OK;
}

// GMPVideoDecoderCallbackProxy
void
GMPRemoveTest::Decoded(GMPVideoi420Frame* aDecodedFrame)
{
  aDecodedFrame->Destroy();
  mDecodeResult = GMPNoErr;
  mTestMonitor.SetFinished();
}

// GMPVideoDecoderCallbackProxy
void
GMPRemoveTest::Error(GMPErr aError)
{
  mDecodeResult = aError;
  mTestMonitor.SetFinished();
}

// GMPVideoDecoderCallbackProxy
void
GMPRemoveTest::Terminated()
{
  mIsTerminated = true;
}

void
GMPRemoveTest::GeneratePlugin()
{
  nsresult rv;
  nsCOMPtr<nsIFile> gmpDir;
  nsCOMPtr<nsIFile> origDir;
  nsCOMPtr<nsIFile> tmpDir;

  rv = NS_GetSpecialDirectory(NS_GRE_DIR,
                              getter_AddRefs(gmpDir));
  EXPECT_OK(rv);
  rv = gmpDir->Append(GMP_DIR_NAME);
  EXPECT_OK(rv);

  rv = gmpDir->Clone(getter_AddRefs(origDir));
  EXPECT_OK(rv);
  rv = origDir->Append(GMP_OLD_VERSION);
  EXPECT_OK(rv);

  rv = origDir->CopyTo(gmpDir, GMP_NEW_VERSION);
  EXPECT_OK(rv);

  rv = gmpDir->Clone(getter_AddRefs(tmpDir));
  EXPECT_OK(rv);
  rv = tmpDir->Append(GMP_NEW_VERSION);
  EXPECT_OK(rv);

  EXPECT_OK(origDir->GetPath(mOriginalPath));
  EXPECT_OK(tmpDir->GetPath(mTmpPath));
  mTmpDir = tmpDir;
}
