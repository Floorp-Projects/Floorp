/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_encodingutils_h_
#define mozilla_dom_encodingutils_h_

#include "nsDataHashtable.h"
#include "nsString.h"

class nsIUnicodeDecoder;
class nsIUnicodeEncoder;

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
   * @param      aOutEncoding, returning corresponding encoding for label.
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
   * Like FindEncodingForLabel() except labels that map to "replacement"
   * are treated as unknown.
   *
   * @param      aLabel, incoming label describing charset to be decoded.
   * @param      aOutEncoding, returning corresponding encoding for label.
   * @return     false if no encoding was found for label.
   *             true if valid encoding found.
   */
  static bool FindEncodingForLabelNoReplacement(const nsACString& aLabel,
                                                nsACString& aOutEncoding);

  static bool FindEncodingForLabelNoReplacement(const nsAString& aLabel,
                                                nsACString& aOutEncoding)
  {
    return FindEncodingForLabelNoReplacement(NS_ConvertUTF16toUTF8(aLabel),
                                             aOutEncoding);
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

  /**
   * Instantiates a decoder for an encoding. The input must be a
   * Gecko-canonical encoding name.
   * @param aEncoding a Gecko-canonical encoding name
   * @return a decoder
   */
  static already_AddRefed<nsIUnicodeDecoder>
  DecoderForEncoding(const char* aEncoding)
  {
    nsDependentCString encoding(aEncoding);
    return DecoderForEncoding(encoding);
  }

  /**
   * Instantiates a decoder for an encoding. The input must be a
   * Gecko-canonical encoding name
   * @param aEncoding a Gecko-canonical encoding name
   * @return a decoder
   */
  static already_AddRefed<nsIUnicodeDecoder>
  DecoderForEncoding(const nsACString& aEncoding);

  /**
   * Instantiates an encoder for an encoding. The input must be a
   * Gecko-canonical encoding name.
   * @param aEncoding a Gecko-canonical encoding name
   * @return an encoder
   */
  static already_AddRefed<nsIUnicodeEncoder>
  EncoderForEncoding(const char* aEncoding)
  {
    nsDependentCString encoding(aEncoding);
    return EncoderForEncoding(encoding);
  }

  /**
   * Instantiates an encoder for an encoding. The input must be a
   * Gecko-canonical encoding name.
   * @param aEncoding a Gecko-canonical encoding name
   * @return an encoder
   */
  static already_AddRefed<nsIUnicodeEncoder>
  EncoderForEncoding(const nsACString& aEncoding);

  /**
   * Finds a Gecko language group string (e.g. x-western) for a Gecko-canonical
   * encoding name.
   *
   * @param      aEncoding, incoming label describing charset to be decoded.
   * @param      aOutGroup, returning corresponding language group.
   */
  static void LangGroupForEncoding(const nsACString& aEncoding,
                                   nsACString& aOutGroup);

private:
  EncodingUtils() = delete;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_encodingutils_h_
