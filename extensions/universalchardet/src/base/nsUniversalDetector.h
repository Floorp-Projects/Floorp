/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUniversalDetector_h__
#define nsUniversalDetector_h__

class nsCharSetProber;

typedef enum { ePureAscii = 0, eEscAscii = 1, eHighbyte = 2 } nsInputState;

class nsUniversalDetector {
 public:
  nsUniversalDetector();
  virtual ~nsUniversalDetector();
  virtual nsresult HandleData(const char* aBuf, uint32_t aLen);
  virtual void DataEnd(void);

 protected:
  virtual void Report(const char* aCharset) = 0;
  virtual void Reset();
  nsInputState mInputState;
  bool mDone;
  bool mInTag;
  bool mStart;
  bool mGotData;
  char mLastChar;
  const char* mDetectedCharset;
  int32_t mBestGuess;
  uint32_t mLanguageFilter;

  nsCharSetProber* mMultibyteProber;
  nsCharSetProber* mEscCharSetProber;
};

#endif
