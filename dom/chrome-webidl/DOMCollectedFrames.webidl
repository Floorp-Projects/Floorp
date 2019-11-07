/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A single frame collected by the |CompositionRecorder|.
 */
[GenerateConversionToJS]
dictionary DOMCollectedFrame {
    /**
     * The offset of the frame past the start point of the recording (in
     * milliseconds).
     */
    required double timeOffset;
    /** A data: URI containing the PNG image data of the frame. */
    required ByteString dataUri;
};

/**
 * Information about frames collected by the |CompositionRecorder|.
 */
[GenerateConversionToJS]
dictionary DOMCollectedFrames {
    /** The collected frames. */
    required sequence<DOMCollectedFrame> frames;
    /** The start point of the recording (in milliseconds). */
    required double recordingStart;
};
