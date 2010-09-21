// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdarg.h>

#include "v8.h"

#include "platform.h"

#include "sys/stat.h"

namespace v8 {
namespace internal {

void StringBuilder::AddString(const char* s) {
  AddSubstring(s, StrLength(s));
}


void StringBuilder::AddSubstring(const char* s, int n) {
  ASSERT(!is_finalized() && position_ + n < buffer_.length());
  ASSERT(static_cast<size_t>(n) <= strlen(s));
  memcpy(&buffer_[position_], s, n * kCharSize);
  position_ += n;
}


// MOZ: This is not from V8.  See DoubleToCString() for details.
void StringBuilder::AddInteger(int n) {
  ASSERT(!is_finalized() && position_ < buffer_.length());
  // Get the number of digits. 
  int ndigits = 0;
  int n2 = n;
  do {
    ndigits++;
    n2 /= 10; 
  } while (n2);

  // Add the integer string backwards.
  position_ += ndigits;
  int i = position_;
  do {
    buffer_[--i] = '0' + (n % 10);
    n /= 10;
  } while (n);
}


void StringBuilder::AddPadding(char c, int count) {
  for (int i = 0; i < count; i++) {
    AddCharacter(c);
  }
}


char* StringBuilder::Finalize() {
  ASSERT(!is_finalized() && position_ < buffer_.length());
  buffer_[position_] = '\0';
  // Make sure nobody managed to add a 0-character to the
  // buffer while building the string.
  ASSERT(strlen(buffer_.start()) == static_cast<size_t>(position_));
  position_ = -1;
  ASSERT(is_finalized());
  return buffer_.start();
}

} }  // namespace v8::internal
