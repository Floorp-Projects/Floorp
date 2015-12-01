/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(OmxPlatformLayer_h_)
#define OmxPlatformLayer_h_

#include "OMX_Core.h"
#include "OMX_Types.h"
#include "mozilla/MozPromise.h"
#include "mozilla/TaskQueue.h"
#include "OmxPromiseLayer.h"

namespace mozilla {

class TrackInfo;

/*
 * This class the the abstract layer of the platform OpenMax IL implementation.
 *
 * For some platform like andoird, it exposures its OpenMax IL via IOMX which 
 * is definitions are different comparing to standard.
 * For other platforms like Raspberry Pi, it will be easy to implement this layer
 * with the standard OpenMax IL api.
 */
class OmxPlatformLayer {
public:
  typedef OmxPromiseLayer::BUFFERLIST BUFFERLIST;
  typedef OmxPromiseLayer::BufferData BufferData;

  virtual OMX_ERRORTYPE InitOmxToStateLoaded(const TrackInfo* aInfo) = 0;

  virtual OMX_ERRORTYPE EmptyThisBuffer(BufferData* aData) = 0;

  virtual OMX_ERRORTYPE FillThisBuffer(BufferData* aData) = 0;

  virtual OMX_ERRORTYPE SendCommand(OMX_COMMANDTYPE aCmd,
                                    OMX_U32 aParam1,
                                    OMX_PTR aCmdData) = 0;

  // Buffer could be platform dependent; for example, video decoding needs gralloc
  // on Gonk. Therefore, derived class needs to implement its owned buffer
  // allocate/release API according to its platform type.
  virtual nsresult AllocateOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBufferList) = 0;

  virtual nsresult ReleaseOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBufferList) = 0;

  virtual OMX_ERRORTYPE GetState(OMX_STATETYPE* aType) = 0;

  virtual OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE aParamIndex,
                                     OMX_PTR aComponentParameterStructure,
                                     OMX_U32 aComponentParameterSize) = 0;

  virtual OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nIndex,
                                     OMX_PTR aComponentParameterStructure,
                                     OMX_U32 aComponentParameterSize) = 0;

  virtual nsresult Shutdown() = 0;

  virtual ~OmxPlatformLayer() {}
};

}

#endif // OmxPlatformLayer_h_
