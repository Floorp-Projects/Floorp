/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_encodingutils_h_
#define mozilla_dom_encodingutils_h_

#include "nsDataHashtable.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class EncodingUtils
{
public:

  /**
   * Implements get an encoding algorithm from Encoding spec.
   * http://encoding.spec.whatwg.org/#concept-encoding-get
   * Given a label, this function returns the corresponding encoding or a
   * false.
   * The returned name may not be lowercased due to compatibility with
   * our internal implementations.
   *
   * @param      aLabel, incoming label describing charset to be decoded.
   * @param      aRetEncoding, returning corresponding encoding for label.
   * @return     false if no encoding was found for label.
   *             true if valid encoding found.
   */
  static bool FindEncodingForLabel(const nsACString& aLabel,
                                   nsACString& aOutEncoding);

  static bool FindEncodingForLabel(const nsAString& aLabel,
                                   nsACString& aOutEncoding)
  {
    return FindEncodingForLabel(NS_ConvertUTF16toUTF8(aLabel), aOutEncoding);
  }

  /**
   * Remove any leading and trailing space characters, following the
   * definition of space characters from Encoding spec.
   * http://encoding.spec.whatwg.org/#terminology
   * Note that nsAString::StripWhitespace() doesn't exactly match the
   * definition. It also removes all matching chars in the string,
   * not just leading and trailing.
   *
   * @param      aString, string to be trimmed.
   */
  template<class T>
  static void TrimSpaceCharacters(T& aString)
  {
    aString.Trim(" \t\n\f\r");
  }

  /**
   * Check is the encoding is ASCII-compatible in the sense that Basic Latin
   * encodes to ASCII bytes. (The reverse may not be true!)
   *
   * @param aPreferredName a preferred encoding label
   * @return whether the encoding is ASCII-compatible
   */
  static bool IsAsciiCompatible(const nsACString& aPreferredName);

private:
  EncodingUtils() MOZ_DELETE;
};

} // dom
} // mozilla

#endif // mozilla_dom_encodingutils_h_
