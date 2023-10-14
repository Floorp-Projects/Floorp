/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SINF_PARSER_H_
#define SINF_PARSER_H_

#include "mozilla/ResultExtensions.h"
#include "Atom.h"
#include "AtomType.h"
#include "nsTArray.h"

namespace mozilla {

class Box;

class Sinf : public Atom {
 public:
  Sinf()
      : mDefaultIVSize(0),

        mDefaultCryptByteBlock(0),
        mDefaultSkipByteBlock(0) {}
  explicit Sinf(Box& aBox);

  bool IsValid() override {
    return !!mDefaultEncryptionType &&  // Should have an encryption scheme
           (mDefaultIVSize > 0 ||       // and either a default IV size
            mDefaultConstantIV.Length() > 0);  // or a constant IV.
  }

  uint8_t mDefaultIVSize;
  AtomType mDefaultEncryptionType;
  uint8_t mDefaultKeyID[16];
  uint8_t mDefaultCryptByteBlock;
  uint8_t mDefaultSkipByteBlock;
  CopyableTArray<uint8_t> mDefaultConstantIV;
};

class SinfParser {
 public:
  explicit SinfParser(Box& aBox);

  Sinf& GetSinf() { return mSinf; }

 private:
  Result<Ok, nsresult> ParseSchm(Box& aBox);
  Result<Ok, nsresult> ParseSchi(Box& aBox);
  Result<Ok, nsresult> ParseTenc(Box& aBox);

  Sinf mSinf;
};

}  // namespace mozilla

#endif  // SINF_PARSER_H_
