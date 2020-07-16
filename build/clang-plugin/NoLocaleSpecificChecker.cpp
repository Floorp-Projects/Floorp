/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoLocaleSpecificChecker.h"
#include <algorithm>
#include "CustomMatchers.h"

void NoLocaleSpecificChecker::registerMatchers(MatchFinder *AstMatcher) {
  auto hasCharPtrParam = [](const unsigned int Position) {
    return hasParameter(
        Position, hasType(hasCanonicalType(pointsTo(asString("char")))));
  };
  auto hasConstCharPtrParam = [](const unsigned int Position) {
    return hasParameter(
        Position, hasType(hasCanonicalType(pointsTo(asString("const char")))));
  };
  auto hasIntegerParam = [](const unsigned int Position) {
    return hasParameter(Position, hasType(isInteger()));
  };
  auto hasNameAndIntegerParam = [&](const char* name) {
    return allOf(hasName(name), hasIntegerParam(0));
  };

  AstMatcher->addMatcher(
      declRefExpr(
          allOf(
              isFirstParty(),
              to(functionDecl(allOf(
                  isInSystemHeader(),
                  anyOf(
                      hasNameAndIntegerParam("isalnum"),
                      hasNameAndIntegerParam("isalpha"),
                      hasNameAndIntegerParam("isblank"),
                      hasNameAndIntegerParam("iscntrl"),
                      hasNameAndIntegerParam("isdigit"),
                      hasNameAndIntegerParam("isgraph"),
                      hasNameAndIntegerParam("islower"),
                      hasNameAndIntegerParam("isprint"),
                      hasNameAndIntegerParam("ispunct"),
                      hasNameAndIntegerParam("isspace"),
                      hasNameAndIntegerParam("isupper"),
                      hasNameAndIntegerParam("isxdigit"),
                      allOf(hasName("strcasecmp"),
                            hasConstCharPtrParam(0),
                            hasConstCharPtrParam(1)),
                      allOf(hasName("strncasecmp"),
                            hasConstCharPtrParam(0),
                            hasConstCharPtrParam(1),
                            hasIntegerParam(2)),
                      allOf(hasName("strcoll"),
                            hasConstCharPtrParam(0),
                            hasConstCharPtrParam(1)),
                      hasNameAndIntegerParam("strerror"),
                      allOf(hasName("strxfrm"),
                            hasCharPtrParam(0),
                            hasConstCharPtrParam(1),
                            hasIntegerParam(2)),
                      hasNameAndIntegerParam("tolower"),
                      hasNameAndIntegerParam("toupper"))))),
              unless(isAllowedToUseLocaleSpecificFunctions())))
      .bind("id"),
      this);
}

struct LocaleSpecificCheckerHint {
    const char* name;
    const char* message;
};

static const LocaleSpecificCheckerHint Hints[] = {
    { "isalnum", "Consider mozilla::IsAsciiAlphanumeric instead." },
    { "isalpha", "Consider mozilla::IsAsciiAlpha instead." },
    { "isdigit", "Consider mozilla::IsAsciiDigit instead." },
    { "islower", "Consider mozilla::IsAsciiLowercaseAlpha instead." },
    { "isspace", "Consider mozilla::IsAsciiWhitespace instead." },
    { "isupper", "Consider mozilla::IsAsciiUppercaseAlpha instead." },
    { "isxdigit", "Consider mozilla::IsAsciiHexDigit instead." },
    { "strcasecmp",
      "Consider mozilla::CompareCaseInsensitiveASCII "
      "or mozilla::EqualsCaseInsensitiveASCII instead." },
    { "strncasecmp",
      "Consider mozilla::CompareCaseInsensitiveASCII "
      "or mozilla::EqualsCaseInsensitiveASCII instead." },
};

void NoLocaleSpecificChecker::check(const MatchFinder::MatchResult &Result) {
  const auto *MatchedDecl = Result.Nodes.getNodeAs<DeclRefExpr>("id");
  diag(MatchedDecl->getExprLoc(),
       "Do not use locale-specific standard library functions.",
       DiagnosticIDs::Warning);

  // Also emit a relevant hint, if we have one.
  std::string Name = MatchedDecl->getNameInfo().getName().getAsString();
  const LocaleSpecificCheckerHint* HintsEnd = Hints + sizeof(Hints) / sizeof(Hints[0]);
  auto Hit = std::find_if(
    Hints,
    HintsEnd,
    [&](const LocaleSpecificCheckerHint& hint) {
      return strcmp(hint.name, Name.c_str()) == 0;
    });
  if (Hit != HintsEnd) {
    diag(MatchedDecl->getExprLoc(), Hit->message, DiagnosticIDs::Note);
  }
}
