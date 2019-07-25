/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ViewportMetaData.h"

#include "mozilla/dom/Document.h"

using namespace mozilla;
using namespace mozilla::dom;

/*
 * Helper function for ViewportMetaData::ProcessViewportInfo.
 *
 * Handles a single key=value pair. If it corresponds to a valid viewport
 * attribute, add it to the document header data. No validation is done on the
 * value itself (this is done at display time).
 */
static void ProcessViewportToken(Document* aDocument, const nsAString& token) {
  /* Iterators. */
  nsAString::const_iterator tip, tail, end;
  token.BeginReading(tip);
  tail = tip;
  token.EndReading(end);

  /* Move tip to the '='. */
  while ((tip != end) && (*tip != '=')) {
    ++tip;
  }

  /* If we didn't find an '=', punt. */
  if (tip == end) {
    return;
  }

  /* Extract the key and value. */
  const nsAString& key = nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(
      Substring(tail, tip), true);
  const nsAString& value = nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(
      Substring(++tip, end), true);

  /* Check for known keys. If we find a match, insert the appropriate
   * information into the document header. */
  RefPtr<nsAtom> key_atom = NS_Atomize(key);
  if (key_atom == nsGkAtoms::height) {
    aDocument->SetHeaderData(nsGkAtoms::viewport_height, value);
  } else if (key_atom == nsGkAtoms::width) {
    aDocument->SetHeaderData(nsGkAtoms::viewport_width, value);
  } else if (key_atom == nsGkAtoms::initial_scale) {
    aDocument->SetHeaderData(nsGkAtoms::viewport_initial_scale, value);
  } else if (key_atom == nsGkAtoms::minimum_scale) {
    aDocument->SetHeaderData(nsGkAtoms::viewport_minimum_scale, value);
  } else if (key_atom == nsGkAtoms::maximum_scale) {
    aDocument->SetHeaderData(nsGkAtoms::viewport_maximum_scale, value);
  } else if (key_atom == nsGkAtoms::user_scalable) {
    aDocument->SetHeaderData(nsGkAtoms::viewport_user_scalable, value);
  }
}

#define IS_SEPARATOR(c)                                             \
  (((c) == '=') || ((c) == ',') || ((c) == ';') || ((c) == '\t') || \
   ((c) == '\n') || ((c) == '\r'))

/* static */
void ViewportMetaData::ProcessViewportInfo(Document* aDocument,
                                           const nsAString& viewportInfo) {
  aDocument->SetHeaderData(nsGkAtoms::viewport, viewportInfo);

  /* Iterators. */
  nsAString::const_iterator tip, tail, end;
  viewportInfo.BeginReading(tip);
  tail = tip;
  viewportInfo.EndReading(end);

  /* Read the tip to the first non-separator character. */
  while ((tip != end) && (IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip))) {
    ++tip;
  }

  /* Read through and find tokens separated by separators. */
  while (tip != end) {
    /* Synchronize tip and tail. */
    tail = tip;

    /* Advance tip past non-separator characters. */
    while ((tip != end) && !IS_SEPARATOR(*tip)) {
      ++tip;
    }

    /* Allow white spaces that surround the '=' character */
    if ((tip != end) && (*tip == '=')) {
      ++tip;

      while ((tip != end) && nsCRT::IsAsciiSpace(*tip)) {
        ++tip;
      }

      while ((tip != end) &&
             !(IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip))) {
        ++tip;
      }
    }

    /* Our token consists of the characters between tail and tip. */
    ProcessViewportToken(aDocument, Substring(tail, tip));

    /* Skip separators. */
    while ((tip != end) && (IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip))) {
      ++tip;
    }
  }
}

#undef IS_SEPARATOR
