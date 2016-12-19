/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MozChecker_h__
#define MozChecker_h__

#include "DiagnosticsMatcher.h"

class MozChecker : public ASTConsumer, public RecursiveASTVisitor<MozChecker> {
  DiagnosticsMatcher Matcher;

public:
  MozChecker(CompilerInstance &CI) :
    Matcher(CI) {}
  virtual ~MozChecker() {}

  ASTConsumerPtr getOtherConsumer() { return Matcher.makeASTConsumer(); }

  static bool hasCustomAnnotation(const Decl *D, const char *Spelling);
};

#endif
