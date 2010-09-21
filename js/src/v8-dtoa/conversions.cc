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

#include <math.h>

#include "v8.h"
#include "dtoa.h"

namespace v8 {
namespace internal {

// MOZ: The return type was changed from 'const char*' to 'char*' to match the
// usage within SpiderMonkey.
//
// MOZ: The arguments were modified to use a char buffer instead of
// v8::internal::Vector, to save SpiderMonkey from having to know about that
// type.
//
// MOZ: The function was modified to return NULL when it needs to fall back to
// Gay's dtoa, rather than calling Gay's dtoa itself.  That's because
// SpiderMonkey already has its own copy of Gay's dtoa.
//
char* DoubleToCString(double v, char* buffer, int buflen) {
  StringBuilder builder(buffer, buflen);

  switch (fpclassify(v)) {
    case FP_NAN:
      builder.AddString("NaN");
      break;

    case FP_INFINITE:
      if (v < 0.0) {
        builder.AddString("-Infinity");
      } else {
        builder.AddString("Infinity");
      }
      break;

    case FP_ZERO:
      builder.AddCharacter('0');
      break;

    default: {
      int decimal_point;
      int sign;
      char* decimal_rep;
      //bool used_gay_dtoa = false;	MOZ: see above
      const int kV8DtoaBufferCapacity = kBase10MaximalLength + 1;
      char v8_dtoa_buffer[kV8DtoaBufferCapacity];
      int length;

      if (DoubleToAscii(v, DTOA_SHORTEST, 0,
                        Vector<char>(v8_dtoa_buffer, kV8DtoaBufferCapacity),
                        &sign, &length, &decimal_point)) {
        decimal_rep = v8_dtoa_buffer;
      } else {
        return NULL;    // MOZ: see above
        //decimal_rep = dtoa(v, 0, 0, &decimal_point, &sign, NULL);
        //used_gay_dtoa = true;
        //length = StrLength(decimal_rep);
      }

      if (sign) builder.AddCharacter('-');

      if (length <= decimal_point && decimal_point <= 21) {
        // ECMA-262 section 9.8.1 step 6.
        builder.AddString(decimal_rep);
        builder.AddPadding('0', decimal_point - length);

      } else if (0 < decimal_point && decimal_point <= 21) {
        // ECMA-262 section 9.8.1 step 7.
        builder.AddSubstring(decimal_rep, decimal_point);
        builder.AddCharacter('.');
        builder.AddString(decimal_rep + decimal_point);

      } else if (decimal_point <= 0 && decimal_point > -6) {
        // ECMA-262 section 9.8.1 step 8.
        builder.AddString("0.");
        builder.AddPadding('0', -decimal_point);
        builder.AddString(decimal_rep);

      } else {
        // ECMA-262 section 9.8.1 step 9 and 10 combined.
        builder.AddCharacter(decimal_rep[0]);
        if (length != 1) {
          builder.AddCharacter('.');
          builder.AddString(decimal_rep + 1);
        }
        builder.AddCharacter('e');
        builder.AddCharacter((decimal_point >= 0) ? '+' : '-');
        int exponent = decimal_point - 1;
        if (exponent < 0) exponent = -exponent;
        // MOZ: This was a call to 'AddFormatted("%d", exponent)', which
        // called onto vsnprintf().  Because this was the only call to
        // AddFormatted in the imported code, it was replaced with this call
        // to AddInteger, which is faster and doesn't require any
        // platform-specific code.
        builder.AddInteger(exponent);
      }

      //if (used_gay_dtoa) freedtoa(decimal_rep);   MOZ: see above
    }
  }
  return builder.Finalize();
}

} }  // namespace v8::internal
