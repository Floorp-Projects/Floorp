// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_TRANSITION_TYPES_H__
#define CHROME_COMMON_PAGE_TRANSITION_TYPES_H__

#include "base/basictypes.h"
#include "base/logging.h"

// This class is for scoping only.
class PageTransition {
 public:
  // Types of transitions between pages. These are stored in the history
  // database to separate visits, and are reported by the renderer for page
  // navigations.
  //
  // WARNING: don't change these numbers. They are written directly into the
  // history database, so future versions will need the same values to match
  // the enums.
  //
  // A type is made of a core value and a set of qualifiers. A type has one
  // core value and 0 or or more qualifiers.
  enum {
    // User got to this page by clicking a link on another page.
    LINK = 0,

    // User got this page by typing the URL in the URL bar.  This should not be
    // used for cases where the user selected a choice that didn't look at all
    // like a URL; see GENERATED below.
    //
    // We also use this for other "explicit" navigation actions.
    TYPED = 1,

    // User got to this page through a suggestion in the UI, for example,
    // through the destinations page.
    AUTO_BOOKMARK = 2,

    // This is a subframe navigation. This is any content that is automatically
    // loaded in a non-toplevel frame. For example, if a page consists of
    // several frames containing ads, those ad URLs will have this transition
    // type. The user may not even realize the content in these pages is a
    // separate frame, so may not care about the URL (see MANUAL below).
    AUTO_SUBFRAME = 3,

    // For subframe navigations that are explicitly requested by the user and
    // generate new navigation entries in the back/forward list. These are
    // probably more important than frames that were automatically loaded in
    // the background because the user probably cares about the fact that this
    // link was loaded.
    MANUAL_SUBFRAME = 4,

    // User got to this page by typing in the URL bar and selecting an entry
    // that did not look like a URL.  For example, a match might have the URL
    // of a Google search result page, but appear like "Search Google for ...".
    // These are not quite the same as TYPED navigations because the user
    // didn't type or see the destination URL.
    // See also KEYWORD.
    GENERATED = 5,

    // The page was specified in the command line or is the start page.
    START_PAGE = 6,

    // The user filled out values in a form and submitted it. NOTE that in
    // some situations submitting a form does not result in this transition
    // type. This can happen if the form uses script to submit the contents.
    FORM_SUBMIT = 7,

    // The user "reloaded" the page, either by hitting the reload button or by
    // hitting enter in the address bar.  NOTE: This is distinct from the
    // concept of whether a particular load uses "reload semantics" (i.e.
    // bypasses cached data).  For this reason, lots of code needs to pass
    // around the concept of whether a load should be treated as a "reload"
    // separately from their tracking of this transition type, which is mainly
    // used for proper scoring for consumers who care about how frequently a
    // user typed/visited a particular URL.
    //
    // SessionRestore and undo tab close use this transition type too.
    RELOAD = 8,

    // The url was generated from a replaceable keyword other than the default
    // search provider. If the user types a keyword (which also applies to
    // tab-to-search) in the omnibox this qualifier is applied to the transition
    // type of the generated url. TemplateURLModel then may generate an
    // additional visit with a transition type of KEYWORD_GENERATED against the
    // url 'http://' + keyword. For example, if you do a tab-to-search against
    // wikipedia the generated url has a transition qualifer of KEYWORD, and
    // TemplateURLModel generates a visit for 'wikipedia.org' with a transition
    // type of KEYWORD_GENERATED.
    KEYWORD = 9,

    // Corresponds to a visit generated for a keyword. See description of
    // KEYWORD for more details.
    KEYWORD_GENERATED = 10,

    // ADDING NEW CORE VALUE? Be sure to update the LAST_CORE and CORE_MASK
    // values below.
    LAST_CORE = KEYWORD_GENERATED,
    CORE_MASK = 0xFF,

    // Qualifiers
    // Any of the core values above can be augmented by one or more qualifiers.
    // These qualifiers further define the transition.

    // The beginning of a navigation chain.
    CHAIN_START = 0x10000000,

    // The last transition in a redirect chain.
    CHAIN_END = 0x20000000,

    // Redirects caused by JavaScript or a meta refresh tag on the page.
    CLIENT_REDIRECT = 0x40000000,

    // Redirects sent from the server by HTTP headers. It might be nice to
    // break this out into 2 types in the future, permanent or temporary, if we
    // can get that information from WebKit.
    SERVER_REDIRECT = 0x80000000,

    // Used to test whether a transition involves a redirect.
    IS_REDIRECT_MASK = 0xC0000000,

    // General mask defining the bits used for the qualifiers.
    QUALIFIER_MASK = 0xFFFFFF00
  };

  // The type used for the bitfield.
  typedef unsigned int Type;

  static bool ValidType(int32_t type) {
    Type t = StripQualifier(static_cast<Type>(type));
    return (t >= 0 && t <= LAST_CORE);
  }

  static Type FromInt(int32_t type) {
    if (!ValidType(type)) {
      NOTREACHED() << "Invalid transition type " << type;

      // Return a safe default so we don't have corrupt data in release mode.
      return LINK;
    }
    return static_cast<Type>(type);
  }

  // Returns true if the given transition is a top-level frame transition, or
  // false if the transition was for a subframe.
  static bool IsMainFrame(Type type) {
    int32_t t = StripQualifier(type);
    return (t != AUTO_SUBFRAME && t != MANUAL_SUBFRAME);
  }

  // Returns whether a transition involves a redirection
  static bool IsRedirect(Type type) {
    return (type & IS_REDIRECT_MASK) != 0;
  }

  // Simplifies the provided transition by removing any qualifier
  static Type StripQualifier(Type type) {
    return static_cast<Type>(type & ~QUALIFIER_MASK);
  }

  // Return the qualifier
  static int32_t GetQualifier(Type type) {
    return type & QUALIFIER_MASK;
  }
};

#endif  // CHROME_COMMON_PAGE_TRANSITION_TYPES_H__
