/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonMemMovableTemplateArgChecker.h"
#include "CustomMatchers.h"

void NonMemMovableTemplateArgChecker::registerMatchers(MatchFinder* AstMatcher) {
  // Handle non-mem-movable template specializations
  AstMatcher->addMatcher(
      classTemplateSpecializationDecl(
          allOf(needsMemMovableTemplateArg(),
                hasAnyTemplateArgument(refersToType(isNonMemMovable()))))
          .bind("specialization"),
      this);
}

void NonMemMovableTemplateArgChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char* Error =
      "Cannot instantiate %0 with non-memmovable template argument %1";
  const char* Note =
      "instantiation of %0 requested here";

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
      diag(Specialization->getLocation(), Error,
           DiagnosticIDs::Error) << Specialization
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
      diag(RequestLoc, Note, DiagnosticIDs::Note) << Specialization;
      NonMemMovable.dumpAnnotationReason(*this, ArgType, RequestLoc);
    }
  }
}
