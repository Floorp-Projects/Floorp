/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
interface nsISupports;

[Pref="media.webspeech.recognition.enable",
 Constructor(DOMString type, optional SpeechRecognitionEventInit eventInitDict)]
interface SpeechRecognitionEvent : Event
{
  readonly attribute unsigned long resultIndex;
  readonly attribute nsISupports? results;
  readonly attribute DOMString? interpretation;
  readonly attribute Document? emma;
};

dictionary SpeechRecognitionEventInit : EventInit
{
  unsigned long resultIndex = 0;
  nsISupports? results = null;
  DOMString interpretation = "";
  Document? emma = null;
};
