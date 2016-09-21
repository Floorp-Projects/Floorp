/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_MediaIPCUtils_h
#define mozilla_dom_media_MediaIPCUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "PlatformDecoderModule.h"

namespace IPC {
  template<>
  struct ParamTraits<mozilla::VideoInfo>
  {
    typedef mozilla::VideoInfo paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
      // TrackInfo
      WriteParam(aMsg, aParam.mMimeType);

      // VideoInfo
      WriteParam(aMsg, aParam.mDisplay);
      WriteParam(aMsg, aParam.mStereoMode);
      WriteParam(aMsg, aParam.mImage);
      WriteParam(aMsg, aParam.ImageRect());
    }

    static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
    {
      nsIntRect imageRect;
      if (ReadParam(aMsg, aIter, &aResult->mMimeType) &&
          ReadParam(aMsg, aIter, &aResult->mDisplay) &&
          ReadParam(aMsg, aIter, &aResult->mStereoMode) &&
          ReadParam(aMsg, aIter, &aResult->mImage) &&
          ReadParam(aMsg, aIter, &imageRect)) {
        aResult->SetImageRect(imageRect);
        return true;
      }
      return false;
    }
  };
} // namespace IPC

#endif // mozilla_dom_media_MediaIPCUtils_h
