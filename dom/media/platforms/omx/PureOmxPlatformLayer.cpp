/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxDataDecoder.h"
#include "OmxPromiseLayer.h"
#include "PureOmxPlatformLayer.h"
#include "OmxCoreLibLinker.h"

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("PureOmxPlatformLayer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define LOG_BUF(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("PureOmxBufferData(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

#define OMX_FUNC(func) extern typeof(func)* func;
#include "OmxFunctionList.h"
#undef OMX_FUNC

PureOmxBufferData::PureOmxBufferData(const PureOmxPlatformLayer& aPlatformLayer,
                                     const OMX_PARAM_PORTDEFINITIONTYPE& aPortDef)
  : BufferData(nullptr)
  , mPlatformLayer(aPlatformLayer)
  , mPortDef(aPortDef)
{
  LOG_BUF("");

  if (ShouldUseEGLImage()) {
    // TODO
    LOG_BUF("OMX_UseEGLImage() seems available but using it isn't implemented yet.");
  }

  OMX_ERRORTYPE err;
  err = OMX_AllocateBuffer(mPlatformLayer.GetComponent(),
                           &mBuffer,
                           mPortDef.nPortIndex,
                           this,
                           mPortDef.nBufferSize);
  if (err != OMX_ErrorNone) {
    LOG_BUF("Failed to allocate the buffer!: 0x%08x", err);
  }
}

PureOmxBufferData::~PureOmxBufferData()
{
  LOG_BUF("");
  ReleaseBuffer();
}

void PureOmxBufferData::ReleaseBuffer()
{
  LOG_BUF("");

  if (mBuffer) {
    OMX_ERRORTYPE err;
    err = OMX_FreeBuffer(mPlatformLayer.GetComponent(),
                         mPortDef.nPortIndex,
                         mBuffer);
    if (err != OMX_ErrorNone) {
      LOG_BUF("Failed to free the buffer!: 0x%08x", err);
    }
    mBuffer = nullptr;
  }
}

bool PureOmxBufferData::ShouldUseEGLImage()
{
  OMX_ERRORTYPE err;
  err = OMX_UseEGLImage(mPlatformLayer.GetComponent(),
                        nullptr,
                        mPortDef.nPortIndex,
                        nullptr,
                        nullptr);
  return (err != OMX_ErrorNotImplemented);
}

/* static */ bool
PureOmxPlatformLayer::Init(void)
{
  if (!OmxCoreLibLinker::Link()) {
    return false;
  }

  OMX_ERRORTYPE err = OMX_Init();
  if (err != OMX_ErrorNone) {
    MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug,
            ("PureOmxPlatformLayer::%s: Failed to initialize OMXCore: 0x%08x",
             __func__, err));
    return false;
  }

  return true;
}

/* static */ OMX_CALLBACKTYPE PureOmxPlatformLayer::sCallbacks =
  { EventHandler, EmptyBufferDone, FillBufferDone };

PureOmxPlatformLayer::PureOmxPlatformLayer(OmxDataDecoder* aDataDecoder,
                                           OmxPromiseLayer* aPromiseLayer,
                                           TaskQueue* aTaskQueue,
                                           layers::ImageContainer* aImageContainer)
  : mComponent(nullptr)
  , mDataDecoder(aDataDecoder)
  , mPromiseLayer(aPromiseLayer)
  , mTaskQueue(aTaskQueue)
  , mImageContainer(aImageContainer)
{
  LOG("");
}

PureOmxPlatformLayer::~PureOmxPlatformLayer()
{
  LOG("");
  if (mComponent) {
    OMX_FreeHandle(mComponent);
  }
}

OMX_ERRORTYPE
PureOmxPlatformLayer::InitOmxToStateLoaded(const TrackInfo* aInfo)
{
  LOG("");

  if (!aInfo) {
    return OMX_ErrorUndefined;
  }
  mInfo = aInfo;

  return CreateComponent();
}

OMX_ERRORTYPE
PureOmxPlatformLayer::EmptyThisBuffer(BufferData* aData)
{
  LOG("");
  return OMX_EmptyThisBuffer(mComponent, aData->mBuffer);
}

OMX_ERRORTYPE
PureOmxPlatformLayer::FillThisBuffer(BufferData* aData)
{
  LOG("");
  return OMX_FillThisBuffer(mComponent, aData->mBuffer);
}

OMX_ERRORTYPE
PureOmxPlatformLayer::SendCommand(OMX_COMMANDTYPE aCmd,
                                  OMX_U32 aParam1,
                                  OMX_PTR aCmdData)
{
  LOG("aCmd: 0x%08x", aCmd);
  if (!mComponent) {
    return OMX_ErrorUndefined;
  }
  return OMX_SendCommand(mComponent, aCmd, aParam1, aCmdData);
}

nsresult
PureOmxPlatformLayer::FindPortDefinition(OMX_DIRTYPE aType,
                                         OMX_PARAM_PORTDEFINITIONTYPE& portDef)
{
  nsTArray<uint32_t> portIndex;
  GetPortIndices(portIndex);
  for (auto idx : portIndex) {
    InitOmxParameter(&portDef);
    portDef.nPortIndex = idx;

    OMX_ERRORTYPE err;
    err = GetParameter(OMX_IndexParamPortDefinition,
                       &portDef,
                       sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    if (err != OMX_ErrorNone) {
      return NS_ERROR_FAILURE;
    } else if (portDef.eDir == aType) {
      LOG("Found OMX_IndexParamPortDefinition: port: %d, type: %d",
          portDef.nPortIndex, portDef.eDir);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
PureOmxPlatformLayer::AllocateOmxBuffer(OMX_DIRTYPE aType,
                                        BUFFERLIST* aBufferList)
{
  LOG("aType: %d", aType);

  OMX_PARAM_PORTDEFINITIONTYPE portDef;
  nsresult result = FindPortDefinition(aType, portDef);
  if (result != NS_OK) {
    return result;
  }

  LOG("nBufferCountActual: %d, nBufferSize: %d",
      portDef.nBufferCountActual, portDef.nBufferSize);

  for (OMX_U32 i = 0; i < portDef.nBufferCountActual; ++i) {
    RefPtr<PureOmxBufferData> buffer = new PureOmxBufferData(*this, portDef);
    aBufferList->AppendElement(buffer);
  }

  return NS_OK;
}

nsresult
PureOmxPlatformLayer::ReleaseOmxBuffer(OMX_DIRTYPE aType,
                                       BUFFERLIST* aBufferList)
{
  LOG("aType: 0x%08x", aType);

  uint32_t len = aBufferList->Length();
  for (uint32_t i = 0; i < len; i++) {
    PureOmxBufferData* buffer =
      static_cast<PureOmxBufferData*>(aBufferList->ElementAt(i).get());

    // All raw OpenMAX buffers have to be released here to flush
    // OMX_CommandStateSet for switching the state to OMX_StateLoaded.
    // See OmxDataDecoder::DoAsyncShutdown() for more detail.
    buffer->ReleaseBuffer();
  }
  aBufferList->Clear();

  return NS_OK;
}

OMX_ERRORTYPE
PureOmxPlatformLayer::GetState(OMX_STATETYPE* aType)
{
  LOG("");

  if (mComponent) {
    return OMX_GetState(mComponent, aType);
  }

  return OMX_ErrorUndefined;
}

OMX_ERRORTYPE
PureOmxPlatformLayer::GetParameter(OMX_INDEXTYPE aParamIndex,
                                   OMX_PTR aComponentParameterStructure,
                                   OMX_U32 aComponentParameterSize)
{
  LOG("aParamIndex: 0x%08x", aParamIndex);

  if (!mComponent) {
    return OMX_ErrorUndefined;
  }

  return OMX_GetParameter(mComponent,
                          aParamIndex,
                          aComponentParameterStructure);
}

OMX_ERRORTYPE
PureOmxPlatformLayer::SetParameter(OMX_INDEXTYPE aParamIndex,
                                   OMX_PTR aComponentParameterStructure,
                                   OMX_U32 aComponentParameterSize)
{
  LOG("aParamIndex: 0x%08x", aParamIndex);

  if (!mComponent) {
    return OMX_ErrorUndefined;
  }

  return OMX_SetParameter(mComponent,
                          aParamIndex,
                          aComponentParameterStructure);
}

nsresult
PureOmxPlatformLayer::Shutdown()
{
  LOG("");
  return NS_OK;
}

/* static */ OMX_ERRORTYPE
PureOmxPlatformLayer::EventHandler(OMX_HANDLETYPE hComponent,
                                   OMX_PTR pAppData,
                                   OMX_EVENTTYPE eEventType,
                                   OMX_U32 nData1,
                                   OMX_U32 nData2,
                                   OMX_PTR pEventData)
{
  PureOmxPlatformLayer* self = static_cast<PureOmxPlatformLayer*>(pAppData);
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction(
        "mozilla::PureOmxPlatformLayer::EventHandler",
        [self, eEventType, nData1, nData2, pEventData] () {
          self->EventHandler(eEventType, nData1, nData2, pEventData);
        });
  nsresult rv = self->mTaskQueue->Dispatch(r.forget());
  return NS_SUCCEEDED(rv) ? OMX_ErrorNone : OMX_ErrorUndefined;
}

/* static */ OMX_ERRORTYPE
PureOmxPlatformLayer::EmptyBufferDone(OMX_HANDLETYPE hComponent,
                                      OMX_IN OMX_PTR pAppData,
                                      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
  PureOmxPlatformLayer* self = static_cast<PureOmxPlatformLayer*>(pAppData);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "mozilla::PureOmxPlatformLayer::EmptyBufferDone",
      [self, pBuffer] () {
        self->EmptyBufferDone(pBuffer);
      });
  nsresult rv = self->mTaskQueue->Dispatch(r.forget());
  return NS_SUCCEEDED(rv) ? OMX_ErrorNone : OMX_ErrorUndefined;
}

/* static */ OMX_ERRORTYPE
PureOmxPlatformLayer::FillBufferDone(OMX_OUT OMX_HANDLETYPE hComponent,
                                     OMX_OUT OMX_PTR pAppData,
                                     OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  PureOmxPlatformLayer* self = static_cast<PureOmxPlatformLayer*>(pAppData);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "mozilla::PureOmxPlatformLayer::FillBufferDone",
      [self, pBuffer] () {
        self->FillBufferDone(pBuffer);
      });
  nsresult rv = self->mTaskQueue->Dispatch(r.forget());
  return NS_SUCCEEDED(rv) ? OMX_ErrorNone : OMX_ErrorUndefined;
}

OMX_ERRORTYPE
PureOmxPlatformLayer::EventHandler(OMX_EVENTTYPE eEventType,
                                   OMX_U32 nData1,
                                   OMX_U32 nData2,
                                   OMX_PTR pEventData)
{
  bool handled = mPromiseLayer->Event(eEventType, nData1, nData2);
  LOG("eEventType: 0x%08x, handled: %d", eEventType, handled);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE
PureOmxPlatformLayer::EmptyBufferDone(OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
  PureOmxBufferData* buffer = static_cast<PureOmxBufferData*>(pBuffer->pAppPrivate);
  OMX_DIRTYPE portDirection = buffer->GetPortDirection();
  LOG("PortDirection: %d", portDirection);
  mPromiseLayer->EmptyFillBufferDone(portDirection, buffer);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE
PureOmxPlatformLayer::FillBufferDone(OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  PureOmxBufferData* buffer = static_cast<PureOmxBufferData*>(pBuffer->pAppPrivate);
  OMX_DIRTYPE portDirection = buffer->GetPortDirection();
  LOG("PortDirection: %d", portDirection);
  mPromiseLayer->EmptyFillBufferDone(portDirection, buffer);
  return OMX_ErrorNone;
}

bool
PureOmxPlatformLayer::SupportsMimeType(const nsACString& aMimeType)
{
  return FindStandardComponent(aMimeType, nullptr);
}

static bool
GetStandardComponentRole(const nsACString& aMimeType,
                         nsACString& aRole)
{
  if (aMimeType.EqualsLiteral("video/avc") ||
      aMimeType.EqualsLiteral("video/mp4") ||
      aMimeType.EqualsLiteral("video/mp4v-es")) {
    aRole.Assign("video_decoder.avc");
    return true;
  } else if (aMimeType.EqualsLiteral("audio/mp4a-latm") ||
             aMimeType.EqualsLiteral("audio/mp4") ||
             aMimeType.EqualsLiteral("audio/aac")) {
    aRole.Assign("audio_decoder.aac");
    return true;
  }
  return false;
}

/* static */ bool
PureOmxPlatformLayer::FindStandardComponent(const nsACString& aMimeType,
                                            nsACString* aComponentName)
{
  nsAutoCString role;
  if (!GetStandardComponentRole(aMimeType, role))
    return false;

  OMX_U32 nComponents = 0;
  OMX_ERRORTYPE err;
  err = OMX_GetComponentsOfRole(const_cast<OMX_STRING>(role.Data()),
                                &nComponents, nullptr);
  if (err != OMX_ErrorNone || nComponents <= 0) {
    return false;
  }
  if (!aComponentName) {
    return true;
  }

  // TODO:
  // Only the first component will be used.
  // We should detect the most preferred component.
  OMX_U8* componentNames[1];
  UniquePtr<OMX_U8[]> componentName;
  componentName = MakeUniqueFallible<OMX_U8[]>(OMX_MAX_STRINGNAME_SIZE);
  componentNames[0] = componentName.get();
  nComponents = 1;
  err = OMX_GetComponentsOfRole(const_cast<OMX_STRING>(role.Data()),
                                &nComponents, componentNames);
  if (err == OMX_ErrorNone) {
    MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug,
            ("PureOmxPlatformLayer::%s: A component has been found for %s: %s",
             __func__, aMimeType.Data(), componentNames[0]));
    aComponentName->Assign(reinterpret_cast<char*>(componentNames[0]));
  }

  return err == OMX_ErrorNone;
}

OMX_ERRORTYPE
PureOmxPlatformLayer::CreateComponent(const nsACString* aComponentName)
{
  nsAutoCString componentName;
  if (aComponentName) {
    componentName = *aComponentName;
  } else if (!FindStandardComponent(mInfo->mMimeType, &componentName)) {
    return OMX_ErrorComponentNotFound;
  }

  OMX_ERRORTYPE err;
  err = OMX_GetHandle(&mComponent,
                      const_cast<OMX_STRING>(componentName.Data()),
                      this,
                      &sCallbacks);

  const char* mime = mInfo->mMimeType.Data();
  if (err == OMX_ErrorNone) {
    LOG("Succeeded to create the component for %s", mime);
  } else {
    LOG("Failed to create the component for %s: 0x%08x", mime, err);
  }

  return err;
}

}
