/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/2017/CR-webrtc-20171102/
 */

// WebIDL note: RTCRtpContrinbutingSource and RTCRtpSynchronizationSource
// are specified currently as interfaces in the spec, however there is an
// open issue which has yet to land changing them to dictionaries.
// See: https://github.com/w3c/webrtc-pc/issues/1533
// and https://bugzilla.mozilla.org/show_bug.cgi?id=1419093

dictionary RTCRtpContributingSource {
    DOMHighResTimeStamp  timestamp;
    unsigned long        source;
    byte?                audioLevel;
};

dictionary RTCRtpSynchronizationSource {
    DOMHighResTimeStamp  timestamp;
    unsigned long        source;
    byte                 audioLevel;
};

/* Hidden shared representation of Contributing and Synchronization sources */
enum RTCRtpSourceEntryType {
    "contributing",
    "synchronization",
};

dictionary RTCRtpSourceEntry {
    DOMHighResTimeStamp    timestamp;
    unsigned long          source;
    byte?                  audioLevel;
    RTCRtpSourceEntryType  sourceType;
};
