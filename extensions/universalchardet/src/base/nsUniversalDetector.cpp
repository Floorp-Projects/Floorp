/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"

#include "nsUniversalDetector.h"

#include "nsMBCSGroupProber.h"
#include "nsEscCharsetProber.h"

nsUniversalDetector::nsUniversalDetector() {
  mDone = false;
  mBestGuess = -1;  // illegal value as signal
  mInTag = false;
  mMultibyteProber = nullptr;
  mEscCharSetProber = nullptr;

  mStart = true;
  mDetectedCharset = nullptr;
  mGotData = false;
  mInputState = ePureAscii;
  mLastChar = '\0';
}

nsUniversalDetector::~nsUniversalDetector() {
  delete mMultibyteProber;
  delete mEscCharSetProber;
}

void nsUniversalDetector::Reset() {
  mDone = false;
  mBestGuess = -1;  // illegal value as signal
  mInTag = false;

  mStart = true;
  mDetectedCharset = nullptr;
  mGotData = false;
  mInputState = ePureAscii;
  mLastChar = '\0';

  if (mMultibyteProber) {
    mMultibyteProber->Reset();
  }

  if (mEscCharSetProber) {
    mEscCharSetProber->Reset();
  }
}

//---------------------------------------------------------------------
#define SHORTCUT_THRESHOLD (float)0.95
#define MINIMUM_THRESHOLD (float)0.20

nsresult nsUniversalDetector::HandleData(const char* aBuf, uint32_t aLen) {
  if (mDone) return NS_OK;

  if (aLen > 0) mGotData = true;

  // If the data starts with BOM, we know it is UTF
  if (mStart) {
    mStart = false;
    if (aLen >= 2) {
      switch (aBuf[0]) {
        case '\xEF':
          if ((aLen > 2) && ('\xBB' == aBuf[1]) && ('\xBF' == aBuf[2])) {
            // EF BB BF  UTF-8 encoded BOM
            mDetectedCharset = "UTF-8";
          }
          break;
        case '\xFE':
          if ('\xFF' == aBuf[1]) {
            // FE FF  UTF-16, big endian BOM
            mDetectedCharset = "UTF-16BE";
          }
          break;
        case '\xFF':
          if ('\xFE' == aBuf[1]) {
            // FF FE  UTF-16, little endian BOM
            mDetectedCharset = "UTF-16LE";
          }
          break;
      }  // switch
    }

    if (mDetectedCharset) {
      mDone = true;
      return NS_OK;
    }
  }

  uint32_t i;
  for (i = 0; i < aLen; i++) {
    // other than 0xa0, if every othe character is ascii, the page is ascii
    if (aBuf[i] & '\x80' &&
        aBuf[i] != '\xA0')  // Since many Ascii only page contains NBSP
    {
      // we got a non-ascii byte (high-byte)
      if (mInputState != eHighbyte) {
        // adjust state
        mInputState = eHighbyte;

        // kill mEscCharSetProber if it is active
        if (mEscCharSetProber) {
          delete mEscCharSetProber;
          mEscCharSetProber = nullptr;
        }

        // start multibyte charset prober
        if (!mMultibyteProber) {
          mMultibyteProber = new nsMBCSGroupProber();
        }
      }
    } else {
      // ok, just pure ascii so far
      if ((ePureAscii == mInputState) && (aBuf[i] == '\033')) {
        // found escape character
        mInputState = eEscAscii;
      }
      mLastChar = aBuf[i];
    }
  }

  nsProbingState st;
  switch (mInputState) {
    case eEscAscii:
      if (nullptr == mEscCharSetProber) {
        mEscCharSetProber = new nsEscCharSetProber();
        if (nullptr == mEscCharSetProber) return NS_ERROR_OUT_OF_MEMORY;
      }
      st = mEscCharSetProber->HandleData(aBuf, aLen);
      if (st == eFoundIt) {
        mDone = true;
        mDetectedCharset = mEscCharSetProber->GetCharSetName();
      }
      break;
    case eHighbyte:
      st = mMultibyteProber->HandleData(aBuf, aLen);
      if (st == eFoundIt) {
        mDone = true;
        mDetectedCharset = mMultibyteProber->GetCharSetName();
        return NS_OK;
      }
      break;

    default:     // pure ascii
              ;  // do nothing here
  }
  return NS_OK;
}

//---------------------------------------------------------------------
void nsUniversalDetector::DataEnd() {
  if (!mGotData) {
    // we haven't got any data yet, return immediately
    // caller program sometimes call DataEnd before anything has been sent to
    // detector
    return;
  }

  if (mDetectedCharset) {
    mDone = true;
    Report(mDetectedCharset);
    return;
  }

  switch (mInputState) {
    case eHighbyte: {
      // do not report anything because we are not confident of it, that's in
      // fact a negative answer
      if (mMultibyteProber->GetConfidence() > MINIMUM_THRESHOLD)
        Report(mMultibyteProber->GetCharSetName());
    } break;
    case eEscAscii:
      break;
    default:;
  }
}
