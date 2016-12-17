/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NeedsNoVTableTypeChecker.h"
#include "CustomMatchers.h"

void NeedsNoVTableTypeChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(
      classTemplateSpecializationDecl(
          allOf(hasAnyTemplateArgument(refersToType(hasVTable())),
                hasNeedsNoVTableTypeAttr()))
          .bind("node"),
      this);
}

void NeedsNoVTableTypeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "%0 cannot be instantiated because %1 has a VTable");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "bad instantiation of %0 requested here");

  const ClassTemplateSpecializationDecl *Specialization =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("node");

  // Get the offending template argument
  QualType Offender;
  const TemplateArgumentList &Args =
      Specialization->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < Args.size(); ++i) {
    Offender = Args[i].getAsType();
    if (typeHasVTable(Offender)) {
      break;
    }
  }

  Diag.Report(Specialization->getLocStart(), ErrorID) << Specialization
                                                      << Offender;
  Diag.Report(Specialization->getPointOfInstantiation(), NoteID)
      << Specialization;
}
