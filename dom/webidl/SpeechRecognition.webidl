/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Constructor,
 Pref="media.webspeech.recognition.enable"]
interface SpeechRecognition : EventTarget {
    // recognition parameters
    [Throws]
    attribute SpeechGrammarList grammars;
    [Throws]
    attribute DOMString lang;
    [Throws]
    attribute boolean continuous;
    [Throws]
    attribute boolean interimResults;
    [Throws]
    attribute unsigned long maxAlternatives;
    [Throws]
    attribute DOMString serviceURI;

    // methods to drive the speech interaction
    [Throws]
    void start();
    void stop();
    void abort();

    // event methods
    attribute EventHandler onaudiostart;
    attribute EventHandler onsoundstart;
    attribute EventHandler onspeechstart;
    attribute EventHandler onspeechend;
    attribute EventHandler onsoundend;
    attribute EventHandler onaudioend;
    attribute EventHandler onresult;
    attribute EventHandler onnomatch;
    attribute EventHandler onerror;
    attribute EventHandler onstart;
    attribute EventHandler onend;
};
