/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * This file declares data structures used to communicate data logged by
 * various components for the purpose of APZ testing (see bug 961289 and 
 * gfx/layers/apz/test/APZTestData.h) to JS test code.
 */

// A single key-value pair in the data.
dictionary ScrollFrameDataEntry {
  DOMString key;
  DOMString value;
};

// All the key-value pairs associated with a given scrollable frame.
// The scrollable frame is identified by a scroll id.
dictionary ScrollFrameData {
  unsigned long long scrollId;
  sequence<ScrollFrameDataEntry> entries;
};

// All the scrollable frames associated with a given paint or repaint request.
// The paint or repaint request is identified by a sequence number.
dictionary APZBucket {
  unsigned long sequenceNumber;
  sequence<ScrollFrameData> scrollFrames;
};

[Pref="apz.test.logging_enabled",
 Exposed=Window]
namespace APZHitResultFlags {
  // These constants should be kept in sync with mozilla::gfx::CompositorHitTestInfo
  const unsigned short INVISIBLE = 0;
  const unsigned short VISIBLE = 0x0001;
  const unsigned short IRREGULAR_AREA = 0x0002;
  const unsigned short APZ_AWARE_LISTENERS = 0x0004;
  const unsigned short INACTIVE_SCROLLFRAME = 0x0008;
  const unsigned short PAN_X_DISABLED = 0x0010;
  const unsigned short PAN_Y_DISABLED = 0x0020;
  const unsigned short PINCH_ZOOM_DISABLED = 0x0040;
  const unsigned short DOUBLE_TAP_ZOOM_DISABLED = 0x0080;
  const unsigned short SCROLLBAR = 0x0100;
  const unsigned short SCROLLBAR_THUMB = 0x0200;
  const unsigned short SCROLLBAR_VERTICAL = 0x0400;
  const unsigned short REQUIRES_TARGET_CONFIRMATION = 0x0800;
};

dictionary APZHitResult {
  float screenX;
  float screenY;
  unsigned short hitResult; // combination of the APZHitResultFlags.* flags
  unsigned long long layersId;
  unsigned long long scrollId;
};

dictionary APZSampledResult {
  float scrollOffsetX;
  float scrollOffsetY;
  DOMHighResTimeStamp sampledTimeStamp;
  unsigned long long layersId;
  unsigned long long scrollId;
};

dictionary AdditionalDataEntry {
  DOMString key;
  DOMString value;
};

// All the paints and repaint requests. This is the top-level data structure.
[GenerateConversionToJS]
dictionary APZTestData {
  sequence<APZBucket> paints;
  sequence<APZBucket> repaintRequests;
  sequence<APZHitResult> hitResults;
  sequence<APZSampledResult> sampledResults;
  sequence<AdditionalDataEntry> additionalData;
};

// A frame uniformity measurement for every scrollable layer
dictionary FrameUniformity {
  unsigned long layerAddress;
  float frameUniformity;
};

[GenerateConversionToJS]
dictionary FrameUniformityResults {
  sequence<FrameUniformity> layerUniformities;
};
