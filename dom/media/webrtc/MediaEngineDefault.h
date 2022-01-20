/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEDEFAULT_H_
#define MEDIAENGINEDEFAULT_H_

#include "nsTArrayForwardDeclare.h"
#include "MediaEngine.h"

namespace mozilla {

template <typename...>
class MediaEventProducer;

/**
 * The default implementation of the MediaEngine interface.
 */
class MediaEngineDefault : public MediaEngine {
 public:
  MediaEngineDefault();

  void EnumerateDevices(dom::MediaSourceEnum, MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void Shutdown() override {}
  RefPtr<MediaEngineSource> CreateSource(const MediaDevice* aDevice) override;

  MediaEventSource<void>& DeviceListChangeEvent() override {
    return mDeviceListChangeEvent;
  }
  bool IsFake() const override { return true; }

 private:
  ~MediaEngineDefault();
  MediaEventProducer<void> mDeviceListChangeEvent;
};

}  // namespace mozilla

#endif /* NSMEDIAENGINEDEFAULT_H_ */
