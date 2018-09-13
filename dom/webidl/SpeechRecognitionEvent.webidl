/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
interface nsISupports;

[Pref="media.webspeech.recognition.enable",
 Func="SpeechRecognition::IsAuthorized",
 Constructor(DOMString type, optional SpeechRecognitionEventInit eventInitDict)]
interface SpeechRecognitionEvent : Event
{
  readonly attribute unsigned long resultIndex;
  readonly attribute SpeechRecognitionResultList? results;
  readonly attribute any interpretation;
  readonly attribute Document? emma;
};

dictionary SpeechRecognitionEventInit : EventInit
{
  unsigned long resultIndex = 0;
  SpeechRecognitionResultList? results = null;
  any interpretation = null;
  Document? emma = null;
};
