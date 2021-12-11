/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FopenUsageChecker.h"
#include "CustomMatchers.h"

void FopenUsageChecker::registerMatchers(MatchFinder *AstMatcher) {

  auto hasConstCharPtrParam = [](const unsigned int Position) {
    return hasParameter(
        Position, hasType(hasCanonicalType(pointsTo(asString("const char")))));
  };

  auto hasParamOfType = [](const unsigned int Position, const char *Name) {
    return hasParameter(Position, hasType(asString(Name)));
  };

  auto hasIntegerParam = [](const unsigned int Position) {
    return hasParameter(Position, hasType(isInteger()));
  };

  AstMatcher->addMatcher(
      callExpr(
          allOf(
              isFirstParty(),
              callee(functionDecl(allOf(
                  isInSystemHeader(),
                  anyOf(
                      allOf(anyOf(allOf(hasName("fopen"),
                                        hasConstCharPtrParam(0)),
                                  allOf(hasName("fopen_s"),
                                        hasParameter(
                                            0, hasType(pointsTo(pointsTo(
                                                   asString("FILE"))))),
                                        hasConstCharPtrParam(2))),
                            hasConstCharPtrParam(1)),
                      allOf(anyOf(hasName("open"),
                                  allOf(hasName("_open"), hasIntegerParam(2)),
                                  allOf(hasName("_sopen"), hasIntegerParam(3))),
                            hasConstCharPtrParam(0), hasIntegerParam(1)),
                      allOf(hasName("_sopen_s"),
                            hasParameter(0, hasType(pointsTo(isInteger()))),
                            hasConstCharPtrParam(1), hasIntegerParam(2),
                            hasIntegerParam(3), hasIntegerParam(4)),
                      allOf(hasName("OpenFile"), hasConstCharPtrParam(0),
                            hasParamOfType(1, "LPOFSTRUCT"),
                            hasIntegerParam(2)),
                      allOf(hasName("CreateFileA"), hasConstCharPtrParam(0),
                            hasIntegerParam(1), hasIntegerParam(2),
                            hasParamOfType(3, "LPSECURITY_ATTRIBUTES"),
                            hasIntegerParam(4), hasIntegerParam(5),
                            hasParamOfType(6, "HANDLE")))))),
              unless(isInWhitelistForFopenUsage())))
          .bind("funcCall"),
      this);
}

void FopenUsageChecker::check(const MatchFinder::MatchResult &Result) {
  const CallExpr *FuncCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");
  static const char *ExtraInfo =
      "On Windows executed functions: fopen, fopen_s, open, _open, _sopen, "
      "_sopen_s, OpenFile, CreateFileA should never be used due to lossy "
      "conversion from UTF8 to ANSI.";

  if (FuncCall) {
    diag(FuncCall->getBeginLoc(),
         "Usage of ASCII file functions (here %0) is forbidden on Windows.",
         DiagnosticIDs::Warning)
        << FuncCall->getDirectCallee()->getName();
    diag(FuncCall->getBeginLoc(), ExtraInfo, DiagnosticIDs::Note);
  }
}
