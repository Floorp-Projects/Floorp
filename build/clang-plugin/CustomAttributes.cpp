/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "CustomAttributes.h"
#include "plugin.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

/* Having annotations in the AST unexpectedly impacts codegen.
 * Ideally, we'd avoid having annotations at all, by using an API such as
 * the one from https://reviews.llvm.org/D31338, and storing the attributes
 * data separately from the AST on our own. Unfortunately, there is no such
 * API currently in clang, so we must do without.
 * We can do something similar, though, where we go through the AST before
 * running the checks, create a mapping of AST nodes to attributes, and
 * remove the attributes/annotations from the AST nodes.
 * Not all declarations can be reached from the decl() AST matcher, though,
 * so we do our best effort (getting the other declarations we look at in
 * checks). We emit a warning when checks look at a note that still has
 * annotations attached (aka, hasn't been seen during our first pass),
 * so that those don't go unnoticed. (-Werror should then take care of
 * making that an error)
 */

using namespace clang;
using namespace llvm;

static DenseMap<const Decl*, CustomAttributesSet> AttributesCache;

static CustomAttributesSet CacheAttributes(const Decl* D)
{
  CustomAttributesSet attrs = {};
  for (auto Attr : D->specific_attrs<AnnotateAttr>()) {
    auto annotation = Attr->getAnnotation();
#define ATTR(a) \
    if (annotation == #a) { \
      attrs.has_ ## a = true; \
    } else
#include "CustomAttributes.inc"
#undef ATTR
    {}
  }
  const_cast<Decl*>(D)->dropAttr<AnnotateAttr>();
  AttributesCache.insert(std::make_pair(D, attrs));
  return attrs;
}

static void Report(const Decl* D, const char* message)
{
  ASTContext& Context = D->getASTContext();
  DiagnosticsEngine& Diag = Context.getDiagnostics();
  unsigned ID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Warning, message);
  Diag.Report(D->getLocStart(), ID);
}

CustomAttributesSet GetAttributes(const Decl* D)
{
  CustomAttributesSet attrs = {};
  if (D->hasAttr<AnnotateAttr>()) {
    Report(D, "Declaration has unhandled annotations.");
    attrs = CacheAttributes(D);
  } else {
    auto attributes = AttributesCache.find(D);
    if (attributes != AttributesCache.end()) {
      attrs = attributes->second;
    }
  }
  return attrs;
}

bool hasCustomAttribute(const clang::Decl* D, CustomAttributes A)
{
  CustomAttributesSet attrs = GetAttributes(D);
  switch (A) {
#define ATTR(a) case a: return attrs.has_ ## a;
#include "CustomAttributes.inc"
#undef ATTR
  }
  return false;
}

class CustomAttributesMatcher : public ast_matchers::MatchFinder::MatchCallback {
public:
  virtual void
  run(const ast_matchers::MatchFinder::MatchResult &Result) final
  {
    if (auto D = Result.Nodes.getNodeAs<Decl>("decl")) {
      CacheAttributes(D);
    } else if (auto L = Result.Nodes.getNodeAs<LambdaExpr>("lambda")) {
      CacheAttributes(L->getCallOperator());
      CacheAttributes(L->getLambdaClass());
    }
  }
};

class CustomAttributesAction : public PluginASTAction {
public:
  ASTConsumerPtr CreateASTConsumer(CompilerInstance &CI,
                                   StringRef FileName) override {
    auto& Context = CI.getASTContext();
    auto AstMatcher = new (Context.Allocate<MatchFinder>()) MatchFinder();
    auto Matcher = new (Context.Allocate<CustomAttributesMatcher>()) CustomAttributesMatcher();
    AstMatcher->addMatcher(decl().bind("decl"), Matcher);
    AstMatcher->addMatcher(lambdaExpr().bind("lambda"), Matcher);
    return AstMatcher->newASTConsumer();
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &Args) override {
    return true;
  }

  ActionType getActionType() override {
    return AddBeforeMainAction;
  }
};

static FrontendPluginRegistry::Add<CustomAttributesAction> X(
  "moz-custom-attributes",
  "prepare custom attributes for moz-check");
