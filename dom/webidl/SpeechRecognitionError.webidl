/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional SpeechRecognitionErrorInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface SpeechRecognitionError : Event
{
  const unsigned long NO_SPEECH = 0;
  const unsigned long ABORTED = 1;
  const unsigned long AUDIO_CAPTURE = 2;
  const unsigned long NETWORK = 3;
  const unsigned long NOT_ALLOWED = 4;
  const unsigned long SERVICE_NOT_ALLOWED = 5;
  const unsigned long BAD_GRAMMAR = 6;
  const unsigned long LANGUAGE_NOT_SUPPORTED = 7;

  readonly attribute unsigned long error;
  readonly attribute DOMString? message;
};

dictionary SpeechRecognitionErrorInit : EventInit
{
  unsigned long error = 0;
  DOMString message = "";
};
