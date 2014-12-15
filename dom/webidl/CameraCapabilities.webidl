/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* The capabilities of the video recorder. These are guaranteed not to change
   over the lifetime of that partcicular instance.
*/
[Func="CameraCapabilities::HasSupport"]
interface CameraRecorderAudioProfile
{
  [Constant, Cached] readonly attribute DOMString codec;
  [Constant, Cached] readonly attribute unsigned long bitsPerSecond;
  [Constant, Cached] readonly attribute unsigned long samplesPerSecond;
  [Constant, Cached] readonly attribute unsigned long channels;

  jsonifier;
};

[Func="CameraCapabilities::HasSupport"]
interface CameraRecorderVideoProfile
{
  [Constant, Cached] readonly attribute DOMString codec;
  [Constant, Cached] readonly attribute unsigned long bitsPerSecond;
  [Constant, Cached] readonly attribute unsigned long framesPerSecond;
  [Constant, Cached] readonly attribute CameraSize size;

  [Constant, Cached] readonly attribute unsigned long width;
  [Constant, Cached] readonly attribute unsigned long height;

  jsonifier;
};

[Func="CameraCapabilities::HasSupport"]
interface CameraRecorderProfile
{
  [Constant, Cached] readonly attribute DOMString name;
  [Constant, Cached] readonly attribute DOMString containerFormat;
  [Constant, Cached] readonly attribute DOMString mimeType;

  [Constant, Cached] readonly attribute CameraRecorderAudioProfile audio;
  [Constant, Cached] readonly attribute CameraRecorderVideoProfile video;

  jsonifier;
};

[Func="CameraCapabilities::HasSupport"]
interface CameraRecorderProfiles
{
  getter CameraRecorderProfile(DOMString profile);

  jsonifier;
};

/* The capabilities of a CameraControl instance. These are guaranteed
   not to change over the lifetime of that particular instance.
*/
[Func="CameraCapabilities::HasSupport"]
interface CameraCapabilities
{
  [Constant, Cached] readonly attribute sequence<CameraSize> previewSizes;
  [Constant, Cached] readonly attribute sequence<CameraSize> pictureSizes;
  [Constant, Cached] readonly attribute sequence<CameraSize> thumbnailSizes;
  [Constant, Cached] readonly attribute sequence<CameraSize> videoSizes;

  [Constant, Cached] readonly attribute sequence<DOMString> fileFormats;

  [Constant, Cached] readonly attribute sequence<DOMString> whiteBalanceModes;
  [Constant, Cached] readonly attribute sequence<DOMString> sceneModes;
  [Constant, Cached] readonly attribute sequence<DOMString> effects;
  [Constant, Cached] readonly attribute sequence<DOMString> flashModes;
  [Constant, Cached] readonly attribute sequence<DOMString> focusModes;

  [Constant, Cached] readonly attribute sequence<double> zoomRatios;

  [Constant, Cached] readonly attribute unsigned long maxFocusAreas;
  [Constant, Cached] readonly attribute unsigned long maxMeteringAreas;
  [Constant, Cached] readonly attribute unsigned long maxDetectedFaces;

  [Constant, Cached] readonly attribute double minExposureCompensation;
  [Constant, Cached] readonly attribute double maxExposureCompensation;
  [Constant, Cached] readonly attribute double exposureCompensationStep;

  [Constant, Cached] readonly attribute CameraRecorderProfiles recorderProfiles;

  [Constant, Cached] readonly attribute sequence<DOMString> isoModes;
  [Constant, Cached] readonly attribute sequence<DOMString> meteringModes;

  jsonifier;
};
