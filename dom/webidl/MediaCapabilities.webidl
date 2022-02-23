/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/media-capabilities/
 *
 * Copyright © 2018 the Contributors to the Media Capabilities Specification
 */

dictionary MediaConfiguration {
  VideoConfiguration video;
  AudioConfiguration audio;
};

dictionary MediaDecodingConfiguration : MediaConfiguration {
  required MediaDecodingType type;
};

dictionary MediaEncodingConfiguration : MediaConfiguration {
  required MediaEncodingType type;
};

enum MediaDecodingType {
  "file",
  "media-source",
};

enum MediaEncodingType {
  "record",
  "transmission"
};

dictionary VideoConfiguration {
  required DOMString contentType;
  required unsigned long width;
  required unsigned long height;
  required unsigned long long bitrate;
  required double framerate;
  boolean hasAlphaChannel;
  HdrMetadataType hdrMetadataType;
  ColorGamut colorGamut;
  TransferFunction transferFunction;
  DOMString scalabilityMode;
};

enum HdrMetadataType {
  "smpteSt2086",
  "smpteSt2094-10",
  "smpteSt2094-40"
};

enum ColorGamut {
  "srgb",
  "p3",
  "rec2020"
};

enum TransferFunction {
  "srgb",
  "pq",
  "hlg"
};

dictionary AudioConfiguration {
  required DOMString contentType;
  DOMString channels;
  unsigned long long bitrate;
  unsigned long samplerate;
};

[Exposed=(Window, Worker), Func="mozilla::dom::MediaCapabilities::Enabled",
 HeaderFile="mozilla/dom/MediaCapabilities.h"]
interface MediaCapabilitiesInfo {
  readonly attribute boolean supported;
  readonly attribute boolean smooth;
  readonly attribute boolean powerEfficient;
};

[Exposed=(Window, Worker), Func="mozilla::dom::MediaCapabilities::Enabled"]
interface MediaCapabilities {
  [NewObject]
  Promise<MediaCapabilitiesInfo> decodingInfo(MediaDecodingConfiguration configuration);
  [NewObject]
  Promise<MediaCapabilitiesInfo> encodingInfo(MediaEncodingConfiguration configuration);
};
