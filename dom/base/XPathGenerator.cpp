/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XPathGenerator.h"

#include "nsGkAtoms.h"
#include "Element.h"
#include "nsTArray.h"

/**
 * Check whether a character is a non-word character. A non-word character is a
 * character that isn't in ('a'..'z') or in ('A'..'Z') or a number or an underscore.
 * */
bool IsNonWordCharacter(const char16_t& aChar)
{
  if (((char16_t('A') <= aChar) && (aChar <= char16_t('Z'))) ||
      ((char16_t('a') <= aChar) && (aChar <= char16_t('z'))) ||
      ((char16_t('0') <= aChar) && (aChar <= char16_t('9'))) ||
      (aChar == char16_t('_'))) {
    return false;
  } else {
    return true;
  }
}

/**
 * Check whether a string contains a non-word character.
 * */
bool ContainNonWordCharacter(const nsAString& aStr)
{
  const char16_t* cur = aStr.BeginReading();
  const char16_t* end = aStr.EndReading();
  for (; cur < end; ++cur) {
    if (IsNonWordCharacter(*cur)) {
      return true;
    }
  }
  return false;
}

/**
 * Get the prefix according to the given namespace and assign the result to aResult.
 * */
void GetPrefix(const nsINode* aNode, nsAString& aResult)
{
  if (aNode->IsXULElement()) {
    aResult.Assign(NS_LITERAL_STRING("xul"));
  } else if (aNode->IsHTMLElement()) {
    aResult.Assign(NS_LITERAL_STRING("xhtml"));
  }
}

void GetNameAttribute(const nsINode* aNode, nsAString& aResult)
{
  if (aNode->HasName()) {
    const Element* elem = aNode->AsElement();
    elem->GetAttr(kNameSpaceID_None, nsGkAtoms::name, aResult);
  }
}

/**
 * Put all sequences of ' in a string in between '," and ",' . And then put
 * the result string in between concat(' and ').
 *
 * For example, a string 'a'' will return result concat('',"'",'a',"''",'')
 * */
void GenerateConcatExpression(const nsAString& aStr, nsAString& aResult)
{
  const char16_t* cur = aStr.BeginReading();
  const char16_t* end = aStr.EndReading();

  // Put all sequences of ' in between '," and ",'
  nsAutoString result;
  const char16_t* nonQuoteBeginPtr = nullptr;
  const char16_t* quoteBeginPtr = nullptr;
  for (; cur < end; ++cur) {
    if (char16_t('\'') == *cur) {
      if (nonQuoteBeginPtr) {
        result.Append(nonQuoteBeginPtr, cur - nonQuoteBeginPtr);
        nonQuoteBeginPtr = nullptr;
      }
      if (!quoteBeginPtr) {
        result.Append(NS_LITERAL_STRING("\',\""));
        quoteBeginPtr = cur;
      }
    } else {
      if (!nonQuoteBeginPtr) {
        nonQuoteBeginPtr = cur;
      }
      if (quoteBeginPtr) {
        result.Append(quoteBeginPtr, cur - quoteBeginPtr);
        result.Append(NS_LITERAL_STRING("\",\'"));
        quoteBeginPtr = nullptr;
      }
    }
  }

  if (quoteBeginPtr) {
    result.Append(quoteBeginPtr, cur - quoteBeginPtr);
    result.Append(NS_LITERAL_STRING("\",\'"));
  } else if (nonQuoteBeginPtr) {
    result.Append(nonQuoteBeginPtr, cur - nonQuoteBeginPtr);
  }

  // Prepend concat(' and append ').
  aResult.Assign(NS_LITERAL_STRING("concat(\'") + result + NS_LITERAL_STRING("\')"));
}

void XPathGenerator::QuoteArgument(const nsAString& aArg, nsAString& aResult)
{
  if (!aArg.Contains('\'')) {
    aResult.Assign(NS_LITERAL_STRING("\'") + aArg + NS_LITERAL_STRING("\'"));
  } else if (!aArg.Contains('\"')) {
    aResult.Assign(NS_LITERAL_STRING("\"") + aArg + NS_LITERAL_STRING("\""));
  } else {
    GenerateConcatExpression(aArg, aResult);
  }
}

void XPathGenerator::EscapeName(const nsAString& aName, nsAString& aResult)
{
  if (ContainNonWordCharacter(aName)) {
    nsAutoString quotedArg;
    QuoteArgument(aName, quotedArg);
    aResult.Assign(NS_LITERAL_STRING("*[local-name()=") + quotedArg + NS_LITERAL_STRING("]"));
  } else {
    aResult.Assign(aName);
  }
}

void XPathGenerator::Generate(const nsINode* aNode, nsAString& aResult)
{
  if (!aNode->GetParentNode()) {
    aResult.Truncate();
    return;
  }

  nsAutoString nodeNamespaceURI;
  aNode->GetNamespaceURI(nodeNamespaceURI);
  const nsString& nodeLocalName = aNode->LocalName();

  nsAutoString prefix;
  nsAutoString tag;
  nsAutoString nodeEscapeName;
  GetPrefix(aNode, prefix);
  EscapeName(nodeLocalName, nodeEscapeName);
  if (prefix.IsEmpty()) {
    tag.Assign(nodeEscapeName);
  } else {
    tag.Assign(prefix + NS_LITERAL_STRING(":") + nodeEscapeName);
  }

  if (aNode->HasID()) {
    // this must be an element
    const Element* elem = aNode->AsElement();
    nsAutoString elemId;
    nsAutoString quotedArgument;
    elem->GetId(elemId);
    QuoteArgument(elemId, quotedArgument);
    aResult.Assign(NS_LITERAL_STRING("//") + tag + NS_LITERAL_STRING("[@id=") +
                   quotedArgument + NS_LITERAL_STRING("]"));
    return;
  }

  int32_t count = 1;
  nsAutoString nodeNameAttribute;
  GetNameAttribute(aNode, nodeNameAttribute);
  for (const Element* e = aNode->GetPreviousElementSibling(); e; e = e->GetPreviousElementSibling()) {
    nsAutoString elementNamespaceURI;
    e->GetNamespaceURI(elementNamespaceURI);
    nsAutoString elementNameAttribute;
    GetNameAttribute(e, elementNameAttribute);
    if (e->LocalName().Equals(nodeLocalName) && elementNamespaceURI.Equals(nodeNamespaceURI) &&
        (nodeNameAttribute.IsEmpty() || elementNameAttribute.Equals(nodeNameAttribute))) {
      ++count;
    }
  }

  nsAutoString namePart;
  nsAutoString countPart;
  if (!nodeNameAttribute.IsEmpty()) {
    nsAutoString quotedArgument;
    QuoteArgument(nodeNameAttribute, quotedArgument);
    namePart.Assign(NS_LITERAL_STRING("[@name=") + quotedArgument + NS_LITERAL_STRING("]"));
  }
  if (count != 1) {
    countPart.Assign(NS_LITERAL_STRING("["));
    countPart.AppendInt(count);
    countPart.Append(NS_LITERAL_STRING("]"));
  }
  Generate(aNode->GetParentNode(), aResult);
  aResult.Append(NS_LITERAL_STRING("/") + tag + namePart + countPart);
}
