/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomTypeAnnotation_h__
#define CustomTypeAnnotation_h__

#include "plugin.h"

class CustomTypeAnnotation {
  enum ReasonKind {
    RK_None,
    RK_Direct,
    RK_ArrayElement,
    RK_BaseClass,
    RK_Field,
    RK_TemplateInherited,
  };
  struct AnnotationReason {
    QualType Type;
    ReasonKind Kind;
    const FieldDecl *Field;

    bool valid() const { return Kind != RK_None; }
  };
  typedef DenseMap<void *, AnnotationReason> ReasonCache;

  const char *Spelling;
  const char *Pretty;
  ReasonCache Cache;

public:
  CustomTypeAnnotation(const char *Spelling, const char *Pretty)
      : Spelling(Spelling), Pretty(Pretty){};

  virtual ~CustomTypeAnnotation() {}

  // Checks if this custom annotation "effectively affects" the given type.
  bool hasEffectiveAnnotation(QualType T) {
    return directAnnotationReason(T).valid();
  }
  void dumpAnnotationReason(DiagnosticsEngine &Diag, QualType T,
                            SourceLocation Loc);

  void reportErrorIfPresent(DiagnosticsEngine &Diag, QualType T,
                            SourceLocation Loc, unsigned ErrorID,
                            unsigned NoteID) {
    if (hasEffectiveAnnotation(T)) {
      Diag.Report(Loc, ErrorID) << T;
      Diag.Report(Loc, NoteID);
      dumpAnnotationReason(Diag, T, Loc);
    }
  }

private:
  bool hasLiteralAnnotation(QualType T) const;
  AnnotationReason directAnnotationReason(QualType T);
  AnnotationReason tmplArgAnnotationReason(ArrayRef<TemplateArgument> Args);

protected:
  // Allow subclasses to apply annotations to external code:
  virtual bool hasFakeAnnotation(const TagDecl *D) const { return false; }
};

extern CustomTypeAnnotation StackClass;
extern CustomTypeAnnotation GlobalClass;
extern CustomTypeAnnotation NonHeapClass;
extern CustomTypeAnnotation HeapClass;
extern CustomTypeAnnotation NonTemporaryClass;
extern CustomTypeAnnotation MustUse;
extern CustomTypeAnnotation NonParam;

#endif
