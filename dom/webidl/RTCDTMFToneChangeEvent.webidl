/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/webrtc/#rtcdtmftonechangeevent
 */

interface RTCDTMFToneChangeEvent : Event {
    constructor(DOMString type,
                optional RTCDTMFToneChangeEventInit eventInitDict = {});

    readonly attribute DOMString tone;
};

dictionary RTCDTMFToneChangeEventInit : EventInit {
    DOMString tone = "";
};
