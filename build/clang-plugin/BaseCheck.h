/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseCheck_h__
#define BaseCheck_h__

class MozContext {};
typedef MozContext ContextType;

class BaseCheck : public MatchFinder::MatchCallback {
public:
  BaseCheck(StringRef CheckName, ContextType* Context) {}
  virtual void registerMatchers(MatchFinder *Finder) {}
  virtual void registerPPCallbacks(CompilerInstance& CI) {}
  virtual void check(const MatchFinder::MatchResult &Result) {}
  DiagnosticBuilder diag(SourceLocation Loc, StringRef Description,
                         DiagnosticIDs::Level Level = DiagnosticIDs::Warning) {
    DiagnosticsEngine &Diag = Context->getDiagnostics();
    unsigned ID = Diag.getDiagnosticIDs()->getCustomDiagID(Level, Description);
    return Diag.Report(Loc, ID);
  }

private:
  void run(const MatchFinder::MatchResult &Result) override {
    Context = Result.Context;
    check(Result);
  }

private:
  ASTContext* Context;
};

#endif
