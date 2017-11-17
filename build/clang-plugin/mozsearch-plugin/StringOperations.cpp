/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StringOperations.h"

static unsigned long djbHash(const char *Str) {
  unsigned long Hash = 5381;

  for (const char *P = Str; *P; P++) {
    // Hash * 33 + c
    Hash = ((Hash << 5) + Hash) + *P;
  }

  return Hash;
}

// This doesn't actually return a hex string of |hash|, but it
// does... something. It doesn't really matter what.
static void hashToString(unsigned long Hash, char *Buffer) {
  const char Table[] = {"0123456789abcdef"};
  char *P = Buffer;
  while (Hash) {
    *P = Table[Hash & 0xf];
    Hash >>= 4;
    P++;
  }

  *P = 0;
}

std::string hash(const std::string &Str) {
  static char HashStr[41];
  unsigned long H = djbHash(Str.c_str());
  hashToString(H, HashStr);
  return std::string(HashStr);
}

std::string toString(int N) {
  return stringFormat("%d", N);
}
