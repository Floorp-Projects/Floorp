/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PureOmxPlatformLayer_h_)
#define PureOmxPlatformLayer_h_

#include "OmxPlatformLayer.h"

namespace mozilla {

class PureOmxPlatformLayer;

class PureOmxBufferData : public OmxPromiseLayer::BufferData
{
protected:
  virtual ~PureOmxBufferData();

public:
  PureOmxBufferData(const PureOmxPlatformLayer& aPlatformLayer,
                    const OMX_PARAM_PORTDEFINITIONTYPE& aPortDef);

  void ReleaseBuffer();
  OMX_DIRTYPE GetPortDirection() const { return mPortDef.eDir; };

protected:
  bool ShouldUseEGLImage();

  const PureOmxPlatformLayer& mPlatformLayer;
  const OMX_PARAM_PORTDEFINITIONTYPE mPortDef;
};

class PureOmxPlatformLayer : public OmxPlatformLayer
{
public:
  static bool Init(void);

  static bool SupportsMimeType(const nsACString& aMimeType);

  PureOmxPlatformLayer(OmxDataDecoder* aDataDecoder,
                       OmxPromiseLayer* aPromiseLayer,
                       TaskQueue* aTaskQueue,
                       layers::ImageContainer* aImageContainer);

  virtual ~PureOmxPlatformLayer();

  OMX_ERRORTYPE InitOmxToStateLoaded(const TrackInfo* aInfo) override;

  OMX_ERRORTYPE EmptyThisBuffer(BufferData* aData) override;

  OMX_ERRORTYPE FillThisBuffer(BufferData* aData) override;

  OMX_ERRORTYPE SendCommand(OMX_COMMANDTYPE aCmd,
                            OMX_U32 aParam1,
                            OMX_PTR aCmdData) override;

  nsresult AllocateOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBufferList) override;

  nsresult ReleaseOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBufferList) override;

  OMX_ERRORTYPE GetState(OMX_STATETYPE* aType) override;

  OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE aParamIndex,
                             OMX_PTR aComponentParameterStructure,
                             OMX_U32 aComponentParameterSize) override;

  OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE aParamIndex,
                             OMX_PTR aComponentParameterStructure,
                             OMX_U32 aComponentParameterSize) override;

  nsresult Shutdown() override;

  OMX_HANDLETYPE GetComponent() const { return mComponent; };

  static OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_EVENTTYPE eEventType,
                                    OMX_U32 nData1,
                                    OMX_U32 nData2,
                                    OMX_PTR pEventData);
  static OMX_ERRORTYPE EmptyBufferDone(OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_PTR pAppData,
                                       OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
  static OMX_ERRORTYPE FillBufferDone(OMX_OUT OMX_HANDLETYPE hComponent,
                                      OMX_OUT OMX_PTR pAppData,
                                      OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

protected:
  static bool FindStandardComponent(const nsACString& aMimeType,
                                    nsACString* aComponentName);

  OMX_ERRORTYPE CreateComponent(const nsACString* aComponentName = nullptr);
  nsresult FindPortDefinition(OMX_DIRTYPE aType,
                              OMX_PARAM_PORTDEFINITIONTYPE& portDef);

  OMX_ERRORTYPE EventHandler(OMX_EVENTTYPE eEventType,
                             OMX_U32 nData1,
                             OMX_U32 nData2,
                             OMX_PTR pEventData);
  OMX_ERRORTYPE EmptyBufferDone(OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
  OMX_ERRORTYPE FillBufferDone(OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

protected:
  static OMX_CALLBACKTYPE sCallbacks;

  OMX_HANDLETYPE mComponent;
  RefPtr<OmxDataDecoder> mDataDecoder;
  RefPtr<OmxPromiseLayer> mPromiseLayer;
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<layers::ImageContainer> mImageContainer;
};

}

#endif // PureOmxPlatformLayer_h_
