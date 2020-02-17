/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This checker implements the "can run script" analysis.  The idea is to detect
 * functions that can run script that are being passed reference-counted
 * arguments (including "this") whose refcount might go to zero as a result of
 * the script running.  We want to prevent that.
 *
 * The approach is to attempt to enforce the following invariants on the call
 * graph:
 *
 * 1) Any caller of a MOZ_CAN_RUN_SCRIPT function is itself MOZ_CAN_RUN_SCRIPT.
 * 2) If a virtual MOZ_CAN_RUN_SCRIPT method overrides a base class method,
 *    that base class method is also MOZ_CAN_RUN_SCRIPT.
 *
 * Invariant 2 ensures that we don't accidentally call a MOZ_CAN_RUN_SCRIPT
 * function via a base-class virtual call.  Invariant 1 ensures that
 * the property of being able to run script propagates up the callstack.  There
 * is an opt-out for invariant 1: A function (declaration _or_ implementation)
 * can be decorated with MOZ_CAN_RUN_SCRIPT_BOUNDARY to indicate that we do not
 * require it or any of its callers to be MOZ_CAN_RUN_SCRIPT even if it calls
 * MOZ_CAN_RUN_SCRIPT functions.
 *
 * There are two known holes in invariant 1, apart from the
 * MOZ_CAN_RUN_SCRIPT_BOUNDARY opt-out:
 *
 *  - Functions called via function pointers can be MOZ_CAN_RUN_SCRIPT even if
 *    their caller is not, because we have no way to determine from the function
 *    pointer what function is being called.
 *  - MOZ_CAN_RUN_SCRIPT destructors can happen in functions that are not
 *    MOZ_CAN_RUN_SCRIPT.
 *    https://bugzilla.mozilla.org/show_bug.cgi?id=1535523 tracks this.
 *
 * Given those invariants we then require that when calling a MOZ_CAN_RUN_SCRIPT
 * function all refcounted arguments (including "this") satisfy one of these
 * conditions:
 *  a) The argument is held via a strong pointer on the stack.
 *  b) The argument is a const strong pointer member of "this".  We know "this"
 *     is being kept alive, and a const strong pointer member can't drop its ref
 *     until "this" dies.
 *  c) The argument is an argument of the caller (and hence held by a strong
 *     pointer somewhere higher up the callstack).
 *  d) The argument is explicitly annotated with MOZ_KnownLive, which indicates
 *     that something is guaranteed to keep it alive (e.g. it's rooted via a JS
 *     reflector).
 *  e) The argument is constexpr and therefore cannot disappear.
 */

#include "CanRunScriptChecker.h"
#include "CustomMatchers.h"
#include "clang/Lex/Lexer.h"

void CanRunScriptChecker::registerMatchers(MatchFinder *AstMatcher) {
  auto Refcounted = qualType(hasDeclaration(cxxRecordDecl(isRefCounted())));
  auto StackSmartPtr =
    ignoreTrivials(
      declRefExpr(to(varDecl(hasAutomaticStorageDuration())),
                  hasType(isSmartPtrToRefCounted())));
  auto ConstMemberOfThisSmartPtr =
    memberExpr(hasType(isSmartPtrToRefCounted()),
               hasType(isConstQualified()),
               hasObjectExpression(cxxThisExpr()));
  // A smartptr can be known-live for three reasons:
  // 1) It's declared on the stack.
  // 2) It's a const member of "this".  We know "this" is alive (recursively)
  //    and const members can't change their value hence can't drop their
  //    reference until "this" gets destroyed.
  // 3) It's an immediate temporary being constructed at the point where the
  //    call is happening.
  auto KnownLiveSmartPtr = anyOf(
    StackSmartPtr,
    ConstMemberOfThisSmartPtr,
    ignoreTrivials(cxxConstructExpr(hasType(isSmartPtrToRefCounted()))));

  auto MozKnownLiveCall =
    ignoreTrivials(callExpr(callee(functionDecl(hasName("MOZ_KnownLive")))));

  // Params of the calling function are presumed live, because it itself should be
  // MOZ_CAN_RUN_SCRIPT.  Note that this is subject to
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1537656 a the moment.
  auto KnownLiveParam = anyOf(
      // "this" is OK
      cxxThisExpr(),
      // A parameter of the calling function is OK.
      declRefExpr(to(parmVarDecl())));

  // A matcher that matches various things that are known to be live directly,
  // without making any assumptions about operators.
  auto KnownLiveBase = anyOf(
      // Things that are known to be a stack or immutable refptr.
      KnownLiveSmartPtr,
      // MOZ_KnownLive() calls.
      MozKnownLiveCall,
      // Params of the caller function.
      KnownLiveParam,
      // Constexpr things.
      declRefExpr(to(varDecl(isConstexpr()))));

  // A matcher that matches various known-live things that don't involve
  // non-unary operators.
  auto KnownLiveSimple = anyOf(
      // Things that are just known live.
      KnownLiveBase,
      // Method calls on a live things that are smart ptrs.  Note that we don't
      // want to allow general method calls on live things, because those can
      // return non-live objects (e.g. consider "live_pointer->foo()" as an
      // example).  For purposes of this analysis we are assuming the method
      // calls on smart ptrs all just return the pointer inside,
      cxxMemberCallExpr(on(
          allOf(hasType(isSmartPtrToRefCounted()),
                KnownLiveBase))),
      // operator* or operator-> on a thing that is already known to be live.
      cxxOperatorCallExpr(
          anyOf(hasOverloadedOperatorName("*"),
                hasOverloadedOperatorName("->")),
          hasAnyArgument(KnownLiveBase),
          argumentCountIs(1)),
      // A dereference on a thing that is known to be live.  This is _not_
      // caught by the "operator* or operator->" clause above, because
      // cxxOperatorCallExpr() only catches cases when a class defines
      // operator*.  The default (built-in) operator* matches unaryOperator()
      // instead.),
      unaryOperator(
          unaryDereferenceOperator(),
          hasUnaryOperand(
              // If we're doing *someArg, the argument of the dereference is an
              // ImplicitCastExpr LValueToRValue which has the DeclRefExpr as an
              // argument.  We could try to match that explicitly with a custom
              // matcher (none of the built-in matchers seem to match on the
              // thing being cast for an implicitCastExpr), but it's simpler to
              // just use ignoreTrivials to strip off the cast.
              ignoreTrivials(KnownLiveBase))),
      // Taking a pointer to a live reference.  We explicitly want to exclude
      // things that are not of type reference-to-refcounted or type refcounted,
      // because if someone takes a pointer to a pointer to refcounted or a
      // pointer to a smart ptr and passes those in to a callee that definitely
      // does not guarantee liveness; in fact the callee could modify those
      // things!  In practice they would be the wrong type anyway, though, so
      // it's hard to add a test for this.
      unaryOperator(
          hasOperatorName("&"),
          hasUnaryOperand(allOf(
              anyOf(
                  hasType(references(Refcounted)),
                  hasType(Refcounted)),
              ignoreTrivials(KnownLiveBase))))
      );

  auto KnownLive = anyOf(
      // Anything above, of course.
      KnownLiveSimple,
      // Conditional operators where both arms are live.
      conditionalOperator(
          hasFalseExpression(ignoreTrivials(KnownLiveSimple)),
          hasTrueExpression(ignoreTrivials(KnownLiveSimple)))
      // We're not handling cases like a dereference of a conditional operator,
      // mostly because handling a dereference in general is so ugly.  I
      // _really_ wish I could just write a recursive matcher here easily.
      );

  auto InvalidArg =
      ignoreTrivialsConditional(
        // We want to consider things if there is anything refcounted involved,
        // including in any of the trivials that we otherwise strip off.
        anyOf(
          hasType(Refcounted),
          hasType(pointsTo(Refcounted)),
          hasType(references(Refcounted)),
          hasType(isSmartPtrToRefCounted())
        ),
        // We want to find any expression,
        expr(
          // which is not known live,
          unless(KnownLive),
          // and which is not a default arg with value nullptr, since those are
          // always safe,
          unless(cxxDefaultArgExpr(isNullDefaultArg())),
          // and which is not a literal nullptr,
          unless(cxxNullPtrLiteralExpr()),
          expr().bind("invalidArg")));

  // A matcher which will mark the first invalid argument it finds invalid, but
  // will always match, even if it finds no invalid arguments, so it doesn't
  // preclude other matchers from running and maybe finding invalid args.
  auto OptionalInvalidExplicitArg = anyOf(
      // We want to find any argument which is invalid.
      hasAnyArgument(InvalidArg),

      // This makes this matcher optional.
      anything());

  // Please note that the hasCanRunScriptAnnotation() matchers are not present
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
                  // or which optionally has an invalid this argument,
                  anyOf(
                    on(InvalidArg),
                    anything()
                  ),
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
/// This class is a callback used internally to match function declarations with
/// the MOZ_CAN_RUN_SCRIPT annotation, adding these functions to the
/// can-run-script function set and making sure the functions they override (if
/// any) also have the annotation.
class FuncSetCallback : public MatchFinder::MatchCallback {
public:
  FuncSetCallback(CanRunScriptChecker& Checker,
                  std::unordered_set<const FunctionDecl *> &FuncSet)
      : CanRunScriptFuncs(FuncSet),
        Checker(Checker) {}

  void run(const MatchFinder::MatchResult &Result) override;

private:
  /// This method checks the methods overriden by the given parameter.
  void checkOverriddenMethods(const CXXMethodDecl *Method);

  std::unordered_set<const FunctionDecl *> &CanRunScriptFuncs;
  CanRunScriptChecker &Checker;
};

void FuncSetCallback::run(const MatchFinder::MatchResult &Result) {
  const FunctionDecl *Func;
  if (auto *Lambda = Result.Nodes.getNodeAs<LambdaExpr>("lambda")) {
    Func = Lambda->getCallOperator();
    if (!Func || !hasCustomAttribute<moz_can_run_script>(Func))
      return;
  } else {
    Func = Result.Nodes.getNodeAs<FunctionDecl>("canRunScriptFunction");
  }

  CanRunScriptFuncs.insert(Func);

  // If this is a method, we check the methods it overrides.
  if (auto *Method = dyn_cast<CXXMethodDecl>(Func)) {
    checkOverriddenMethods(Method);
  }
}

void FuncSetCallback::checkOverriddenMethods(const CXXMethodDecl *Method) {
  for (auto OverriddenMethod : Method->overridden_methods()) {
    if (!hasCustomAttribute<moz_can_run_script>(OverriddenMethod)) {
      const char *ErrorNonCanRunScriptOverridden =
          "functions marked as MOZ_CAN_RUN_SCRIPT cannot override functions "
          "that are not marked MOZ_CAN_RUN_SCRIPT";
      const char* NoteNonCanRunScriptOverridden =
          "overridden function declared here";

      Checker.diag(Method->getLocation(), ErrorNonCanRunScriptOverridden,
                   DiagnosticIDs::Error);
      Checker.diag(OverriddenMethod->getLocation(),
                   NoteNonCanRunScriptOverridden,
                   DiagnosticIDs::Note);
    }
  }
}
} // namespace

void CanRunScriptChecker::buildFuncSet(ASTContext *Context) {
  // We create a match finder.
  MatchFinder Finder;
  // We create the callback which will be called when we find a function with
  // a MOZ_CAN_RUN_SCRIPT annotation.
  FuncSetCallback Callback(*this, CanRunScriptFuncs);
  // We add the matcher to the finder, linking it to our callback.
  Finder.addMatcher(
      functionDecl(hasCanRunScriptAnnotation()).bind("canRunScriptFunction"),
      &Callback);
  Finder.addMatcher(
      lambdaExpr().bind("lambda"),
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
      "arguments must all be strong refs or caller's parameters when calling a "
      "function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object "
      "argument).  '%0' is neither.";

  const char *ErrorNonCanRunScriptParent =
      "functions marked as MOZ_CAN_RUN_SCRIPT can only be called from "
      "functions also marked as MOZ_CAN_RUN_SCRIPT";
  const char *NoteNonCanRunScriptParent = "caller function declared here";

  const Expr *InvalidArg;
  if (const CXXDefaultArgExpr* defaultArg =
          Result.Nodes.getNodeAs<CXXDefaultArgExpr>("invalidArg")) {
    InvalidArg = defaultArg->getExpr();
  } else {
    InvalidArg = Result.Nodes.getNodeAs<Expr>("invalidArg");
  }

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
  //
  // In addition, If the parent function is annotated as a
  // CAN_RUN_SCRIPT_BOUNDARY, we don't want to complain about it calling a
  // CAN_RUN_SCRIPT function. This is a mechanism to opt out of the infectious
  // nature of CAN_RUN_SCRIPT which is necessary in some tricky code like
  // Bindings.
  if (ParentFunction &&
      (CanRunScriptFuncs.count(ParentFunction) ||
       hasCustomAttribute<moz_can_run_script_boundary>(ParentFunction))) {
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
    const std::string invalidArgText =
        Lexer::getSourceText(
            CharSourceRange::getTokenRange(InvalidArg->getSourceRange()),
            Result.Context->getSourceManager(),
            Result.Context->getLangOpts());
    diag(InvalidArg->getExprLoc(), ErrorInvalidArg, DiagnosticIDs::Error)
        << InvalidArg->getSourceRange() << invalidArgText;
  }

  // If the parent function is not marked as MOZ_CAN_RUN_SCRIPT, we emit an
  // error and a not indicating it.
  if (ParentFunction) {
    assert(!hasCustomAttribute<moz_can_run_script>(ParentFunction) &&
           "Matcher missed something");

    diag(CallRange.getBegin(), ErrorNonCanRunScriptParent, DiagnosticIDs::Error)
        << CallRange;

    diag(ParentFunction->getCanonicalDecl()->getLocation(),
         NoteNonCanRunScriptParent, DiagnosticIDs::Note);
  }
}
