/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CustomTypeAnnotation.h"
#include "Utils.h"

CustomTypeAnnotation StackClass =
    CustomTypeAnnotation(moz_stack_class, "stack");
CustomTypeAnnotation GlobalClass =
    CustomTypeAnnotation(moz_global_class, "global");
CustomTypeAnnotation NonHeapClass =
    CustomTypeAnnotation(moz_nonheap_class, "non-heap");
CustomTypeAnnotation HeapClass = CustomTypeAnnotation(moz_heap_class, "heap");
CustomTypeAnnotation NonTemporaryClass =
    CustomTypeAnnotation(moz_non_temporary_class, "non-temporary");
CustomTypeAnnotation TemporaryClass =
    CustomTypeAnnotation(moz_temporary_class, "temporary");
CustomTypeAnnotation StaticLocalClass =
    CustomTypeAnnotation(moz_static_local_class, "static-local");

void CustomTypeAnnotation::dumpAnnotationReason(BaseCheck &Check, QualType T,
                                                SourceLocation Loc) {
  const char *Inherits =
      "%1 is a %0 type because it inherits from a %0 type %2";
  const char *Member = "%1 is a %0 type because member %2 is a %0 type %3";
  const char *Array = "%1 is a %0 type because it is an array of %0 type %2";
  const char *Templ =
      "%1 is a %0 type because it has a template argument %0 type %2";
  const char *Implicit = "%1 is a %0 type because %2";

  AnnotationReason Reason = directAnnotationReason(T);
  for (;;) {
    switch (Reason.Kind) {
    case RK_ArrayElement:
      Check.diag(Loc, Array, DiagnosticIDs::Note) << Pretty << T << Reason.Type;
      break;
    case RK_BaseClass: {
      const CXXRecordDecl *Declaration = T->getAsCXXRecordDecl();
      assert(Declaration && "This type should be a C++ class");

      Check.diag(Declaration->getLocation(), Inherits, DiagnosticIDs::Note)
          << Pretty << T << Reason.Type;
      break;
    }
    case RK_Field:
      Check.diag(Reason.Field->getLocation(), Member, DiagnosticIDs::Note)
          << Pretty << T << Reason.Field << Reason.Type;
      break;
    case RK_TemplateInherited: {
      const CXXRecordDecl *Declaration = T->getAsCXXRecordDecl();
      assert(Declaration && "This type should be a C++ class");

      Check.diag(Declaration->getLocation(), Templ, DiagnosticIDs::Note)
          << Pretty << T << Reason.Type;
      break;
    }
    case RK_Implicit: {
      const TagDecl *Declaration = T->getAsTagDecl();
      assert(Declaration && "This type should be a TagDecl");

      Check.diag(Declaration->getLocation(), Implicit, DiagnosticIDs::Note)
          << Pretty << T << Reason.ImplicitReason;
      return;
    }
    default:
      // FIXME (bug 1203263): note the original annotation.
      return;
    }

    T = Reason.Type;
    Reason = directAnnotationReason(T);
  }
}

CustomTypeAnnotation::AnnotationReason
CustomTypeAnnotation::directAnnotationReason(QualType T) {
  if (const TagDecl *D = T->getAsTagDecl()) {
    if (hasCustomAttribute(D, Attribute)) {
      AnnotationReason Reason = {T, RK_Direct, nullptr, ""};
      return Reason;
    }

    std::string ImplAnnotReason = getImplicitReason(D);
    if (!ImplAnnotReason.empty()) {
      AnnotationReason Reason = {T, RK_Implicit, nullptr, ImplAnnotReason};
      return Reason;
    }
  }

  // Check if we have a cached answer
  void *Key = T.getAsOpaquePtr();
  ReasonCache::iterator Cached = Cache.find(T.getAsOpaquePtr());
  if (Cached != Cache.end()) {
    return Cached->second;
  }

  // Check if we have a type which we can recurse into
  if (const clang::ArrayType *Array = T->getAsArrayTypeUnsafe()) {
    if (hasEffectiveAnnotation(Array->getElementType())) {
      AnnotationReason Reason = {Array->getElementType(), RK_ArrayElement,
                                 nullptr, ""};
      Cache[Key] = Reason;
      return Reason;
    }
  }

  // Recurse into Base classes
  if (const CXXRecordDecl *Declaration = T->getAsCXXRecordDecl()) {
    if (Declaration->hasDefinition()) {
      Declaration = Declaration->getDefinition();

      for (const CXXBaseSpecifier &Base : Declaration->bases()) {
        if (hasEffectiveAnnotation(Base.getType())) {
          AnnotationReason Reason = {Base.getType(), RK_BaseClass, nullptr, ""};
          Cache[Key] = Reason;
          return Reason;
        }
      }

      // Recurse into members
      for (const FieldDecl *Field : Declaration->fields()) {
        if (hasEffectiveAnnotation(Field->getType())) {
          AnnotationReason Reason = {Field->getType(), RK_Field, Field, ""};
          Cache[Key] = Reason;
          return Reason;
        }
      }

      // Recurse into template arguments if the annotation
      // MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS is present
      if (hasCustomAttribute<moz_inherit_type_annotations_from_template_args>(
              Declaration)) {
        const ClassTemplateSpecializationDecl *Spec =
            dyn_cast<ClassTemplateSpecializationDecl>(Declaration);
        if (Spec) {
          const TemplateArgumentList &Args = Spec->getTemplateArgs();

          AnnotationReason Reason = tmplArgAnnotationReason(Args.asArray());
          if (Reason.Kind != RK_None) {
            Cache[Key] = Reason;
            return Reason;
          }
        }
      }
    }
  }

  AnnotationReason Reason = {QualType(), RK_None, nullptr, ""};
  Cache[Key] = Reason;
  return Reason;
}

CustomTypeAnnotation::AnnotationReason
CustomTypeAnnotation::tmplArgAnnotationReason(ArrayRef<TemplateArgument> Args) {
  for (const TemplateArgument &Arg : Args) {
    if (Arg.getKind() == TemplateArgument::Type) {
      QualType Type = Arg.getAsType();
      if (hasEffectiveAnnotation(Type)) {
        AnnotationReason Reason = {Type, RK_TemplateInherited, nullptr, ""};
        return Reason;
      }
    } else if (Arg.getKind() == TemplateArgument::Pack) {
      AnnotationReason Reason = tmplArgAnnotationReason(Arg.getPackAsArray());
      if (Reason.Kind != RK_None) {
        return Reason;
      }
    }
  }

  AnnotationReason Reason = {QualType(), RK_None, nullptr, ""};
  return Reason;
}
