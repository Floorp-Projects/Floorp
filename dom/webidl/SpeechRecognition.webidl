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

[Constructor, PrefControlled]
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
    [SetterThrows]
    attribute EventHandler onaudiostart;
    [SetterThrows]
    attribute EventHandler onsoundstart;
    [SetterThrows]
    attribute EventHandler onspeechstart;
    [SetterThrows]
    attribute EventHandler onspeechend;
    [SetterThrows]
    attribute EventHandler onsoundend;
    [SetterThrows]
    attribute EventHandler onaudioend;
    [SetterThrows]
    attribute EventHandler onresult;
    [SetterThrows]
    attribute EventHandler onnomatch;
    [SetterThrows]
    attribute EventHandler onerror;
    [SetterThrows]
    attribute EventHandler onstart;
    [SetterThrows]
    attribute EventHandler onend;
};
