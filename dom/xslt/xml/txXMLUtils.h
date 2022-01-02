/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An XML Utility class
 **/

#ifndef MITRE_XMLUTILS_H
#define MITRE_XMLUTILS_H

#include "txCore.h"
#include "nsDependentSubstring.h"
#include "txXPathNode.h"

#define kExpatSeparatorChar 0xFFFF

extern "C" int MOZ_XMLIsLetter(const char* ptr);
extern "C" int MOZ_XMLIsNCNameChar(const char* ptr);

class nsAtom;

class XMLUtils {
 public:
  static nsresult splitExpatName(const char16_t* aExpatName, nsAtom** aPrefix,
                                 nsAtom** aLocalName, int32_t* aNameSpaceID);
  static nsresult splitQName(const nsAString& aName, nsAtom** aPrefix,
                             nsAtom** aLocalName);

  /*
   * Returns true if the given character is whitespace.
   */
  static bool isWhitespace(const char16_t& aChar) {
    return (aChar <= ' ' &&
            (aChar == ' ' || aChar == '\r' || aChar == '\n' || aChar == '\t'));
  }

  /**
   * Returns true if the given string has only whitespace characters
   */
  static bool isWhitespace(const nsAString& aText);

  /**
   * Normalizes the value of a XML processingInstruction
   **/
  static void normalizePIValue(nsAString& attValue);

  /**
   * Returns true if the given string is a valid XML QName
   */
  static bool isValidQName(const nsAString& aQName, const char16_t** aColon);

  /**
   * Returns true if the given character represents an Alpha letter
   */
  static bool isLetter(char16_t aChar) {
    return !!MOZ_XMLIsLetter(reinterpret_cast<const char*>(&aChar));
  }

  /**
   * Returns true if the given character is an allowable NCName character
   */
  static bool isNCNameChar(char16_t aChar) {
    return !!MOZ_XMLIsNCNameChar(reinterpret_cast<const char*>(&aChar));
  }

  /*
   * Walks up the document tree and returns true if the closest xml:space
   * attribute is "preserve"
   */
  static bool getXMLSpacePreserve(const txXPathNode& aNode);
};

#endif
