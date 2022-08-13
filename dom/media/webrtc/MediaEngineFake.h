/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEFAKE_H_
#define MEDIAENGINEFAKE_H_

#include "nsTArrayForwardDeclare.h"
#include "MediaEngine.h"

namespace mozilla {

template <typename...>
class MediaEventProducer;

/**
 * The fake implementation of the MediaEngine interface.
 */
class MediaEngineFake : public MediaEngine {
 public:
  MediaEngineFake();

  void EnumerateDevices(dom::MediaSourceEnum, MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void Shutdown() override {}
  RefPtr<MediaEngineSource> CreateSource(const MediaDevice* aDevice) override;

  MediaEventSource<void>& DeviceListChangeEvent() override {
    return mDeviceListChangeEvent;
  }
  bool IsFake() const override { return true; }

 private:
  ~MediaEngineFake();
  MediaEventProducer<void> mDeviceListChangeEvent;
};

}  // namespace mozilla

#endif /* NSMEDIAENGINEFAKE_H_ */
