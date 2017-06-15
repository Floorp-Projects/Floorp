/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPATHGENERATOR_H__
#define XPATHGENERATOR_H__
#include "nsString.h"
#include "nsINode.h"

class XPathGenerator
{
public:
  /**
   * Return a properly quoted string to insert into an XPath
   * */
  static void QuoteArgument(const nsAString& aArg, nsAString& aResult);

  /**
   * Return a valid XPath for the given node (usually the local name itself)
   * */
  static void EscapeName(const nsAString& aName, nsAString& aResult);

  /**
   * Generate an approximate XPath query to an (X)HTML node
   * */
  static void Generate(const nsINode* aNode, nsAString& aResult);
};

#endif
