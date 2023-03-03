/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-highlight-api-1/
 *
 * Copyright © 2021 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

/**
 * Enum defining the available highlight types.
 * See https://drafts.csswg.org/css-highlight-api-1/#enumdef-highlighttype
 */
enum HighlightType {
  "highlight",
  "spelling-error",
  "grammar-error"
};

/**
 * Definition of a highlight object, consisting of a set of ranges,
 * a priority and a highlight type.
 *
 * See https://drafts.csswg.org/css-highlight-api-1/#highlight
 */
[Pref="dom.customHighlightAPI.enabled", Exposed=Window]
interface Highlight {

  [Throws]
  constructor(AbstractRange... initialRanges);
  setlike<AbstractRange>;
  attribute long priority;
  attribute HighlightType type;
};

partial interface Highlight {
  // Setlike methods need to be overridden.
  // Iterating a setlike is not possible from C++ yet.
  // Therefore a separate data structure must be held and kept in sync.
  [Throws]
  undefined add(AbstractRange range);
  [Throws]
  undefined clear();
  [Throws]
  boolean delete(AbstractRange range);
};

/**
 * Registry object that contains all Highlights associated with a Document.
 *
 * See https://drafts.csswg.org/css-highlight-api-1/#highlightregistry
 */
[Pref="dom.customHighlightAPI.enabled", Exposed=Window]
interface HighlightRegistry {
  maplike<DOMString, Highlight>;
};

partial interface HighlightRegistry {
  // Maplike interface methods need to be overridden.
  // Iterating a maplike is not possible from C++ yet.
  // Therefore, a separate data structure must be held and kept in sync.
  [Throws]
  undefined set(DOMString key, Highlight value);
  [Throws]
  undefined clear();
  [Throws]
  boolean delete(DOMString key);
};
