/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"

#define CLANG_VERSION_FULL (CLANG_VERSION_MAJOR * 100 + CLANG_VERSION_MINOR)

using namespace llvm;
using namespace clang;

namespace {
class MozChecker : public ASTConsumer, public RecursiveASTVisitor<MozChecker> {
  DiagnosticsEngine &Diag;
  const CompilerInstance &CI;
public:
  MozChecker(const CompilerInstance &CI) : Diag(CI.getDiagnostics()), CI(CI) {}
  virtual void HandleTranslationUnit(ASTContext &ctx) {
    TraverseDecl(ctx.getTranslationUnitDecl());
  }

  static bool hasCustomAnnotation(const Decl *d, const char *spelling) {
    AnnotateAttr *attr = d->getAttr<AnnotateAttr>();
    if (!attr)
      return false;

    return attr->getAnnotation() == spelling;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *d) {
    // We need definitions, not declarations
    if (!d->isThisDeclarationADefinition()) return true;

    // Look through all of our immediate bases to find methods that need to be
    // overridden
    typedef std::vector<CXXMethodDecl *> OverridesVector;
    OverridesVector must_overrides;
    for (CXXRecordDecl::base_class_iterator base = d->bases_begin(),
         e = d->bases_end(); base != e; ++base) {
      // The base is either a class (CXXRecordDecl) or it's a templated class...
      CXXRecordDecl *parent = base->getType()
        .getDesugaredType(d->getASTContext())->getAsCXXRecordDecl();
      // The parent might not be resolved to a type yet. In this case, we can't
      // do any checking here. For complete correctness, we should visit
      // template instantiations, but this case is likely to be rare, so we will
      // ignore it until it becomes important.
      if (!parent) {
        continue;
      }
      parent = parent->getDefinition();
      for (CXXRecordDecl::method_iterator M = parent->method_begin();
          M != parent->method_end(); ++M) {
        if (hasCustomAnnotation(*M, "moz_must_override"))
          must_overrides.push_back(*M);
      }
    }

    for (OverridesVector::iterator it = must_overrides.begin();
        it != must_overrides.end(); ++it) {
      bool overridden = false;
      for (CXXRecordDecl::method_iterator M = d->method_begin();
          !overridden && M != d->method_end(); ++M) {
        // The way that Clang checks if a method M overrides its parent method
        // is if the method has the same name but would not overload.
        if (M->getName() == (*it)->getName() &&
            !CI.getSema().IsOverload(*M, (*it), false))
          overridden = true;
      }
      if (!overridden) {
        unsigned overrideID = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error, "%0 must override %1");
        unsigned overrideNote = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note, "function to override is here");
        Diag.Report(d->getLocation(), overrideID) << d->getDeclName() <<
          (*it)->getDeclName();
        Diag.Report((*it)->getLocation(), overrideNote);
      }
    }
    return true;
  }
};

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumer *CreateASTConsumer(CompilerInstance &CI, StringRef fileName) {
    return new MozChecker(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) {
    return true;
  }
};
}

static FrontendPluginRegistry::Add<MozCheckAction>
X("moz-check", "check moz action");
