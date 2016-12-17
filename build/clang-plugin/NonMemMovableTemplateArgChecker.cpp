/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonMemMovableTemplateArgChecker.h"
#include "CustomMatchers.h"

void NonMemMovableTemplateArgChecker::registerMatcher(MatchFinder& AstMatcher) {
  // Handle non-mem-movable template specializations
  AstMatcher.addMatcher(
      classTemplateSpecializationDecl(
          allOf(needsMemMovableTemplateArg(),
                hasAnyTemplateArgument(refersToType(isNonMemMovable()))))
          .bind("specialization"),
      this);
}

void NonMemMovableTemplateArgChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Cannot instantiate %0 with non-memmovable template argument %1");
  unsigned Note1ID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "instantiation of %0 requested here");

  // Get the specialization
  const ClassTemplateSpecializationDecl *Specialization =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("specialization");
  SourceLocation RequestLoc = Specialization->getPointOfInstantiation();

  // Report an error for every template argument which is non-memmovable
  const TemplateArgumentList &Args =
      Specialization->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < Args.size(); ++i) {
    QualType ArgType = Args[i].getAsType();
    if (NonMemMovable.hasEffectiveAnnotation(ArgType)) {
      Diag.Report(Specialization->getLocation(), ErrorID) << Specialization
                                                          << ArgType;
      // XXX It would be really nice if we could get the instantiation stack
      // information
      // from Sema such that we could print a full template instantiation stack,
      // however,
      // it seems as though that information is thrown out by the time we get
      // here so we
      // can only report one level of template specialization (which in many
      // cases won't
      // be useful)
      Diag.Report(RequestLoc, Note1ID) << Specialization;
      NonMemMovable.dumpAnnotationReason(Diag, ArgType, RequestLoc);
    }
  }
}
