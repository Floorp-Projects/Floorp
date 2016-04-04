/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef dom_media_platforms_MediaTelemetryConstants_h___
#define dom_media_platforms_MediaTelemetryConstants_h___

namespace mozilla {
namespace media {

enum class MediaDecoderBackend : uint32_t
{
  WMFSoftware = 0,
  WMFDXVA2D3D9 = 1,
  WMFDXVA2D3D11 = 2
};

} // namespace media
} // namespace mozilla

#endif // dom_media_platforms_MediaTelemetryConstants_h___
