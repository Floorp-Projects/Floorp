/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanRunScriptChecker.h"
#include "CustomMatchers.h"

void CanRunScriptChecker::registerMatchers(MatchFinder *AstMatcher) {
  auto InvalidArg =
      // We want to find any expression,
      ignoreTrivials(expr(
          // which has a refcounted pointer type,
          hasType(pointerType(
              pointee(hasDeclaration(cxxRecordDecl(isRefCounted()))))),
          // and which is not this,
          unless(cxxThisExpr()),
          // and which is not a method call on a smart ptr,
          unless(cxxMemberCallExpr(on(hasType(isSmartPtrToRefCounted())))),
          // and which is not a parameter of the parent function,
          unless(declRefExpr(to(parmVarDecl()))),
          // and which is not a MOZ_KnownLive wrapped value.
          unless(callExpr(callee(functionDecl(hasName("MOZ_KnownLive"))))),
          expr().bind("invalidArg")));

  auto OptionalInvalidExplicitArg = anyOf(
      // We want to find any argument which is invalid.
      hasAnyArgument(InvalidArg),

      // This makes this matcher optional.
      anything());

  // Please not that the hasCanRunScriptAnnotation() matchers are not present
  // directly in the cxxMemberCallExpr, callExpr and constructExpr matchers
  // because we check that the corresponding functions can run script later in
  // the checker code.
  AstMatcher->addMatcher(
      expr(
          anyOf(
              // We want to match a method call expression,
              cxxMemberCallExpr(
                  // which optionally has an invalid arg,
                  OptionalInvalidExplicitArg,
                  // or which optionally has an invalid implicit this argument,
                  anyOf(
                      // which derefs into an invalid arg,
                      on(cxxOperatorCallExpr(
                          anyOf(hasAnyArgument(InvalidArg), anything()))),
                      // or is an invalid arg.
                      on(InvalidArg),

                      anything()),
                  expr().bind("callExpr")),
              // or a regular call expression,
              callExpr(
                  // which optionally has an invalid arg.
                  OptionalInvalidExplicitArg, expr().bind("callExpr")),
              // or a construct expression,
              cxxConstructExpr(
                  // which optionally has an invalid arg.
                  OptionalInvalidExplicitArg, expr().bind("constructExpr"))),

          anyOf(
              // We want to match the parent function.
              forFunction(functionDecl().bind("nonCanRunScriptParentFunction")),

              // ... optionally.
              anything())),
      this);
}

void CanRunScriptChecker::onStartOfTranslationUnit() {
  IsFuncSetBuilt = false;
  CanRunScriptFuncs.clear();
}

namespace {
/// This class is a callback used internally to match function declarations
/// with the MOZ_CAN_RUN_SCRIPT annotation, adding these functions and all
/// the methods they override to the can-run-script function set.
class FuncSetCallback : public MatchFinder::MatchCallback {
public:
  FuncSetCallback(std::unordered_set<const FunctionDecl *> &FuncSet)
      : CanRunScriptFuncs(FuncSet) {}

  void run(const MatchFinder::MatchResult &Result) override;

private:
  /// This method recursively adds all the methods overriden by the given
  /// paremeter.
  void addAllOverriddenMethodsRecursively(const CXXMethodDecl *Method);

  std::unordered_set<const FunctionDecl *> &CanRunScriptFuncs;
};

void FuncSetCallback::run(const MatchFinder::MatchResult &Result) {
  const FunctionDecl *Func =
      Result.Nodes.getNodeAs<FunctionDecl>("canRunScriptFunction");

  CanRunScriptFuncs.insert(Func);

  // If this is a method, we check the methods it overrides.
  if (auto *Method = dyn_cast<CXXMethodDecl>(Func)) {
    addAllOverriddenMethodsRecursively(Method);
  }
}

void FuncSetCallback::addAllOverriddenMethodsRecursively(
    const CXXMethodDecl *Method) {
  for (auto OverriddenMethod : Method->overridden_methods()) {
    CanRunScriptFuncs.insert(OverriddenMethod);

    // If this is not the definition, we also add the definition (if it
    // exists) to the set.
    if (!OverriddenMethod->isThisDeclarationADefinition()) {
      if (auto Def = OverriddenMethod->getDefinition()) {
        CanRunScriptFuncs.insert(Def);
      }
    }

    addAllOverriddenMethodsRecursively(OverriddenMethod);
  }
}
} // namespace

void CanRunScriptChecker::buildFuncSet(ASTContext *Context) {
  // We create a match finder.
  MatchFinder Finder;
  // We create the callback which will be called when we find a function with
  // a MOZ_CAN_RUN_SCRIPT annotation.
  FuncSetCallback Callback(CanRunScriptFuncs);
  // We add the matcher to the finder, linking it to our callback.
  Finder.addMatcher(
      functionDecl(hasCanRunScriptAnnotation()).bind("canRunScriptFunction"),
      &Callback);

  // We start the analysis, given the ASTContext our main checker is in.
  Finder.matchAST(*Context);
}

void CanRunScriptChecker::check(const MatchFinder::MatchResult &Result) {

  // If the set of functions which can run script is not yet built, then build
  // it.
  if (!IsFuncSetBuilt) {
    buildFuncSet(Result.Context);
    IsFuncSetBuilt = true;
  }

  const char *ErrorInvalidArg =
      "arguments must all be strong refs or parent parameters when calling a "
      "function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object "
      "argument)";

  const char *ErrorNonCanRunScriptParent =
      "functions marked as MOZ_CAN_RUN_SCRIPT can only be called from "
      "functions also marked as MOZ_CAN_RUN_SCRIPT";
  const char *NoteNonCanRunScriptParent = "parent function declared here";

  const Expr *InvalidArg = Result.Nodes.getNodeAs<Expr>("invalidArg");

  const CallExpr *Call = Result.Nodes.getNodeAs<CallExpr>("callExpr");
  // If we don't find the FunctionDecl linked to this call or if it's not marked
  // as can-run-script, consider that we didn't find a match.
  if (Call && (!Call->getDirectCallee() ||
               !CanRunScriptFuncs.count(Call->getDirectCallee()))) {
    Call = nullptr;
  }

  const CXXConstructExpr *Construct =
      Result.Nodes.getNodeAs<CXXConstructExpr>("constructExpr");

  // If we don't find the CXXConstructorDecl linked to this construct expression
  // or if it's not marked as can-run-script, consider that we didn't find a
  // match.
  if (Construct && (!Construct->getConstructor() ||
                    !CanRunScriptFuncs.count(Construct->getConstructor()))) {
    Construct = nullptr;
  }

  const FunctionDecl *ParentFunction =
      Result.Nodes.getNodeAs<FunctionDecl>("nonCanRunScriptParentFunction");
  // If the parent function can run script, consider that we didn't find a match
  // because we only care about parent functions which can't run script.
  if (ParentFunction && CanRunScriptFuncs.count(ParentFunction)) {
    ParentFunction = nullptr;
  }

  // Get the call range from either the CallExpr or the ConstructExpr.
  SourceRange CallRange;
  if (Call) {
    CallRange = Call->getSourceRange();
  } else if (Construct) {
    CallRange = Construct->getSourceRange();
  } else {
    // If we have neither a Call nor a Construct, we have nothing do to here.
    return;
  }

  // If we have an invalid argument in the call, we emit the diagnostic to
  // signal it.
  if (InvalidArg) {
    diag(CallRange.getBegin(), ErrorInvalidArg, DiagnosticIDs::Error)
        << CallRange;
  }

  // If the parent function is not marked as MOZ_CAN_RUN_SCRIPT, we emit an
  // error and a not indicating it.
  if (ParentFunction) {
    diag(CallRange.getBegin(), ErrorNonCanRunScriptParent, DiagnosticIDs::Error)
        << CallRange;

    diag(ParentFunction->getLocation(), NoteNonCanRunScriptParent,
         DiagnosticIDs::Note);
  }
}
