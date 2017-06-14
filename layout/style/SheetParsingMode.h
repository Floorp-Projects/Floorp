/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_SheetParsingMode_h
#define mozilla_css_SheetParsingMode_h

namespace mozilla {
namespace css {

/**
 * Enum defining the mode in which a sheet is to be parsed.  This is
 * usually, but not always, the same as the cascade level at which the
 * sheet will apply (see nsStyleSet.h).  Most of the Loader APIs only
 * support loading of author sheets.
 *
 * Author sheets are the normal case: styles embedded in or linked
 * from HTML pages.  They are also the most restricted.
 *
 * User sheets can do anything author sheets can do, and also get
 * access to a few CSS extensions that are not yet suitable for
 * exposure on the public Web, but are very useful for expressing
 * user style overrides, such as @-moz-document rules.
 *
 * XXX: eUserSheetFeatures was added in bug 1035091, but some patches in
 * that bug never landed to use this enum value. Currently, all the features
 * in user sheet are also available in author sheet.
 *
 * Agent sheets have access to all author- and user-sheet features
 * plus more extensions that are necessary for internal use but,
 * again, not yet suitable for exposure on the public Web.  Some of
 * these are outright unsafe to expose; in particular, incorrect
 * styling of anonymous box pseudo-elements can violate layout
 * invariants.
 *
 * Agent sheets that do not use any unsafe rules could use
 * eSafeAgentSheetFeatures when creating the sheet. This enum value allows
 * Servo backend to recognize the sheets as the agent level, but Gecko
 * backend will parse it under _author_ level.
 */
enum SheetParsingMode {
  eAuthorSheetFeatures = 0,
  eUserSheetFeatures,
  eAgentSheetFeatures,
  eSafeAgentSheetFeatures,
};

} // namespace css
} // namespace mozilla

#endif // mozilla_css_SheetParsingMode_h
