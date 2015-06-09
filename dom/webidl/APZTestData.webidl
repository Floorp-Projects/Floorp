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

// All the paints and repaint requests. This is the top-level data structure.
dictionary APZTestData {
  sequence<APZBucket> paints;
  sequence<APZBucket> repaintRequests;
};

// A frame uniformity measurement for every scrollable layer
dictionary FrameUniformity {
  unsigned long layerAddress;
  float frameUniformity;
};

dictionary FrameUniformityResults {
  sequence<FrameUniformity> layerUniformities;
};
