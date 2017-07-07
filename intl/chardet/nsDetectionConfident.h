/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDetetctionConfident_h__
#define nsDetetctionConfident_h__

/*
  This type is used to indicate how confident the detection module about
  the return result.

  eNoAnswerYet is used to indicate that the detector have not find out a
               answer yet based on the data it received.
  eBestAnswer  is used to indicate that the answer the detector returned
               is the best one within the knowledge of the detector.
               In other words, the test to all other candidcates fail.

               For example, the (Shift_JIS/EUC-JP/ISO-2022-JP) detection
               module may return this with answer "Shift_JIS "if it receive
               bytes > 0x80 (which make ISO-2022-JP test failed) and byte
               0x82 (which may EUC-JP test failed)

  eSureAnswer  is used to indicate that the detector is 100% sure about the
               answer.
               Exmaple 1; the Shift_JIS/ISO-2022-JP/EUC-JP detector return
               this w/ ISO-2022-JP when it hit one of the following ESC seq
                  ESC ( J
                  ESC $ @
                  ESC $ B
               Example 2: the detector which can detect UCS2 return w/ UCS2
               when the first 2 byte are BOM mark.
               Example 3: the Korean detector return ISO-2022-KR when it
               hit ESC $ ) C

 */
typedef enum {
  eNoAnswerYet = 0,
  eBestAnswer,
  eSureAnswer,
  eNoAnswerMatch
} nsDetectionConfident;

#endif /* nsDetetctionConfident_h__ */
