/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Utils_h__
#define Utils_h__

#include "plugin.h"

// Check if the given expression contains an assignment expression.
// This can either take the form of a Binary Operator or a
// Overloaded Operator Call.
inline bool hasSideEffectAssignment(const Expr *Expression) {
  if (auto OpCallExpr = dyn_cast_or_null<CXXOperatorCallExpr>(Expression)) {
    auto BinOp = OpCallExpr->getOperator();
    if (BinOp == OO_Equal || (BinOp >= OO_PlusEqual && BinOp <= OO_PipeEqual)) {
      return true;
    }
  } else if (auto BinOpExpr = dyn_cast_or_null<BinaryOperator>(Expression)) {
    if (BinOpExpr->isAssignmentOp()) {
      return true;
    }
  }

  // Recurse to children.
  for (const Stmt *SubStmt : Expression->children()) {
    auto ChildExpr = dyn_cast_or_null<Expr>(SubStmt);
    if (ChildExpr && hasSideEffectAssignment(ChildExpr)) {
      return true;
    }
  }

  return false;
}

template <class T> inline bool ASTIsInSystemHeader(const ASTContext &AC, const T &D) {
  auto &SourceManager = AC.getSourceManager();
  auto ExpansionLoc = SourceManager.getExpansionLoc(D.getLocStart());
  if (ExpansionLoc.isInvalid()) {
    return false;
  }
  return SourceManager.isInSystemHeader(ExpansionLoc);
}

template<typename T>
inline StringRef getNameChecked(const T& D) {
  return D->getIdentifier() ? D->getName() : "";
}

/// A cached data of whether classes are refcounted or not.
typedef DenseMap<const CXXRecordDecl *, std::pair<const Decl *, bool>>
    RefCountedMap;
extern RefCountedMap RefCountedClasses;

inline bool classHasAddRefRelease(const CXXRecordDecl *D) {
  const RefCountedMap::iterator &It = RefCountedClasses.find(D);
  if (It != RefCountedClasses.end()) {
    return It->second.second;
  }

  bool SeenAddRef = false;
  bool SeenRelease = false;
  for (CXXRecordDecl::method_iterator Method = D->method_begin();
       Method != D->method_end(); ++Method) {
    const auto &Name = getNameChecked(Method);
    if (Name == "AddRef") {
      SeenAddRef = true;
    } else if (Name == "Release") {
      SeenRelease = true;
    }
  }
  RefCountedClasses[D] = std::make_pair(D, SeenAddRef && SeenRelease);
  return SeenAddRef && SeenRelease;
}

inline bool isClassRefCounted(QualType T);

inline bool isClassRefCounted(const CXXRecordDecl *D) {
  // Normalize so that D points to the definition if it exists.
  if (!D->hasDefinition())
    return false;
  D = D->getDefinition();
  // Base class: anyone with AddRef/Release is obviously a refcounted class.
  if (classHasAddRefRelease(D))
    return true;

  // Look through all base cases to figure out if the parent is a refcounted
  // class.
  for (CXXRecordDecl::base_class_const_iterator Base = D->bases_begin();
       Base != D->bases_end(); ++Base) {
    bool Super = isClassRefCounted(Base->getType());
    if (Super) {
      return true;
    }
  }

  return false;
}

inline bool isClassRefCounted(QualType T) {
  while (const clang::ArrayType *ArrTy = T->getAsArrayTypeUnsafe())
    T = ArrTy->getElementType();
  CXXRecordDecl *Clazz = T->getAsCXXRecordDecl();
  return Clazz ? isClassRefCounted(Clazz) : false;
}

inline const FieldDecl *getClassRefCntMember(const CXXRecordDecl *D) {
  for (RecordDecl::field_iterator Field = D->field_begin(), E = D->field_end();
       Field != E; ++Field) {
    if (getNameChecked(Field) == "mRefCnt") {
      return *Field;
    }
  }
  return 0;
}

inline bool typeHasVTable(QualType T) {
  while (const clang::ArrayType *ArrTy = T->getAsArrayTypeUnsafe())
    T = ArrTy->getElementType();
  CXXRecordDecl *Offender = T->getAsCXXRecordDecl();
  return Offender && Offender->hasDefinition() && Offender->isDynamicClass();
}

inline std::string getDeclarationNamespace(const Decl *Declaration) {
  const DeclContext *DC =
      Declaration->getDeclContext()->getEnclosingNamespaceContext();
  const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(DC);
  if (!ND) {
    return "";
  }

  while (const DeclContext *ParentDC = ND->getParent()) {
    if (!isa<NamespaceDecl>(ParentDC)) {
      break;
    }
    ND = cast<NamespaceDecl>(ParentDC);
  }

  const auto &Name = ND->getName();
  return Name;
}

inline bool isInIgnoredNamespaceForImplicitCtor(const Decl *Declaration) {
  std::string Name = getDeclarationNamespace(Declaration);
  if (Name == "") {
    return false;
  }

  return Name == "std" ||               // standard C++ lib
         Name == "__gnu_cxx" ||         // gnu C++ lib
         Name == "boost" ||             // boost
         Name == "webrtc" ||            // upstream webrtc
         Name == "rtc" ||               // upstream webrtc 'base' package
         Name.substr(0, 4) == "icu_" || // icu
         Name == "google" ||            // protobuf
         Name == "google_breakpad" ||   // breakpad
         Name == "soundtouch" ||        // libsoundtouch
         Name == "stagefright" ||       // libstagefright
         Name == "MacFileUtilities" ||  // MacFileUtilities
         Name == "dwarf2reader" ||      // dwarf2reader
         Name == "arm_ex_to_module" ||  // arm_ex_to_module
         Name == "testing" ||           // gtest
         Name == "Json";                // jsoncpp
}

inline bool isInIgnoredNamespaceForImplicitConversion(const Decl *Declaration) {
  std::string Name = getDeclarationNamespace(Declaration);
  if (Name == "") {
    return false;
  }

  return Name == "std" ||             // standard C++ lib
         Name == "__gnu_cxx" ||       // gnu C++ lib
         Name == "google_breakpad" || // breakpad
         Name == "testing";           // gtest
}

inline bool isIgnoredPathForImplicitCtor(const Decl *Declaration) {
  SourceLocation Loc = Declaration->getLocation();
  const SourceManager &SM = Declaration->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator Begin = llvm::sys::path::rbegin(FileName),
                                    End = llvm::sys::path::rend(FileName);
  for (; Begin != End; ++Begin) {
    if (Begin->compare_lower(StringRef("skia")) == 0 ||
        Begin->compare_lower(StringRef("sfntly")) == 0 ||
        Begin->compare_lower(StringRef("angle")) == 0 ||
        Begin->compare_lower(StringRef("harfbuzz")) == 0 ||
        Begin->compare_lower(StringRef("hunspell")) == 0 ||
        Begin->compare_lower(StringRef("scoped_ptr.h")) == 0 ||
        Begin->compare_lower(StringRef("graphite2")) == 0 ||
        Begin->compare_lower(StringRef("icu")) == 0 ||
        Begin->compare_lower(StringRef("libcubeb")) == 0 ||
        Begin->compare_lower(StringRef("libstagefright")) == 0 ||
        Begin->compare_lower(StringRef("cairo")) == 0) {
      return true;
    }
    if (Begin->compare_lower(StringRef("chromium")) == 0) {
      // Ignore security/sandbox/chromium but not ipc/chromium.
      ++Begin;
      return Begin != End && Begin->compare_lower(StringRef("sandbox")) == 0;
    }
  }
  return false;
}

inline bool isIgnoredPathForImplicitConversion(const Decl *Declaration) {
  Declaration = Declaration->getCanonicalDecl();
  SourceLocation Loc = Declaration->getLocation();
  const SourceManager &SM = Declaration->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator Begin = llvm::sys::path::rbegin(FileName),
                                    End = llvm::sys::path::rend(FileName);
  for (; Begin != End; ++Begin) {
    if (Begin->compare_lower(StringRef("graphite2")) == 0) {
      return true;
    }
    if (Begin->compare_lower(StringRef("chromium")) == 0) {
      // Ignore security/sandbox/chromium but not ipc/chromium.
      ++Begin;
      return Begin != End && Begin->compare_lower(StringRef("sandbox")) == 0;
    }
  }
  return false;
}

inline bool isIgnoredPathForSprintfLiteral(const CallExpr *Call, const SourceManager &SM) {
  SourceLocation Loc = Call->getLocStart();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator Begin = llvm::sys::path::rbegin(FileName),
                                    End = llvm::sys::path::rend(FileName);
  for (; Begin != End; ++Begin) {
    if (Begin->compare_lower(StringRef("angle")) == 0 ||
        Begin->compare_lower(StringRef("chromium")) == 0 ||
        Begin->compare_lower(StringRef("crashreporter")) == 0 ||
        Begin->compare_lower(StringRef("google-breakpad")) == 0 ||
        Begin->compare_lower(StringRef("gflags")) == 0 ||
        Begin->compare_lower(StringRef("harfbuzz")) == 0 ||
        Begin->compare_lower(StringRef("jsoncpp")) == 0 ||
        Begin->compare_lower(StringRef("libstagefright")) == 0 ||
        Begin->compare_lower(StringRef("mtransport")) == 0 ||
        Begin->compare_lower(StringRef("protobuf")) == 0 ||
        Begin->compare_lower(StringRef("skia")) == 0 ||
        Begin->compare_lower(StringRef("sfntly")) == 0 ||
        // Gtest uses snprintf as GTEST_SNPRINTF_ with sizeof
        Begin->compare_lower(StringRef("testing")) == 0) {
      return true;
    }
    if (Begin->compare_lower(StringRef("webrtc")) == 0) {
      // Ignore trunk/webrtc, but not media/webrtc
      ++Begin;
      return Begin != End && Begin->compare_lower(StringRef("trunk")) == 0;
    }
  }
  return false;
}

inline bool isInterestingDeclForImplicitConversion(const Decl *Declaration) {
  return !isInIgnoredNamespaceForImplicitConversion(Declaration) &&
         !isIgnoredPathForImplicitConversion(Declaration);
}

inline bool isIgnoredExprForMustUse(const Expr *E) {
  if (const CXXOperatorCallExpr *OpCall = dyn_cast<CXXOperatorCallExpr>(E)) {
    switch (OpCall->getOperator()) {
    case OO_Equal:
    case OO_PlusEqual:
    case OO_MinusEqual:
    case OO_StarEqual:
    case OO_SlashEqual:
    case OO_PercentEqual:
    case OO_CaretEqual:
    case OO_AmpEqual:
    case OO_PipeEqual:
    case OO_LessLessEqual:
    case OO_GreaterGreaterEqual:
      return true;
    default:
      return false;
    }
  }

  if (const BinaryOperator *Op = dyn_cast<BinaryOperator>(E)) {
    return Op->isAssignmentOp();
  }

  return false;
}

inline bool typeIsRefPtr(QualType Q) {
  CXXRecordDecl *D = Q->getAsCXXRecordDecl();
  if (!D || !D->getIdentifier()) {
    return false;
  }

  StringRef name = D->getName();
  if (name == "RefPtr" || name == "nsCOMPtr") {
    return true;
  }
  return false;
}

// The method defined in clang for ignoring implicit nodes doesn't work with
// some AST trees. To get around this, we define our own implementation of
// IgnoreTrivials.
inline const Stmt *IgnoreTrivials(const Stmt *s) {
  while (true) {
    if (!s) {
      return nullptr;
    } else if (auto *ewc = dyn_cast<ExprWithCleanups>(s)) {
      s = ewc->getSubExpr();
    } else if (auto *mte = dyn_cast<MaterializeTemporaryExpr>(s)) {
      s = mte->GetTemporaryExpr();
    } else if (auto *bte = dyn_cast<CXXBindTemporaryExpr>(s)) {
      s = bte->getSubExpr();
    } else if (auto *ice = dyn_cast<ImplicitCastExpr>(s)) {
      s = ice->getSubExpr();
    } else if (auto *pe = dyn_cast<ParenExpr>(s)) {
      s = pe->getSubExpr();
    } else {
      break;
    }
  }

  return s;
}

inline const Expr *IgnoreTrivials(const Expr *e) {
  return cast<Expr>(IgnoreTrivials(static_cast<const Stmt *>(e)));
}

const FieldDecl *getBaseRefCntMember(QualType T);

inline const FieldDecl *getBaseRefCntMember(const CXXRecordDecl *D) {
  const FieldDecl *RefCntMember = getClassRefCntMember(D);
  if (RefCntMember && isClassRefCounted(D)) {
    return RefCntMember;
  }

  for (CXXRecordDecl::base_class_const_iterator Base = D->bases_begin(),
                                                E = D->bases_end();
       Base != E; ++Base) {
    RefCntMember = getBaseRefCntMember(Base->getType());
    if (RefCntMember) {
      return RefCntMember;
    }
  }
  return 0;
}

inline const FieldDecl *getBaseRefCntMember(QualType T) {
  while (const clang::ArrayType *ArrTy = T->getAsArrayTypeUnsafe())
    T = ArrTy->getElementType();
  CXXRecordDecl *Clazz = T->getAsCXXRecordDecl();
  return Clazz ? getBaseRefCntMember(Clazz) : 0;
}

inline bool hasCustomAnnotation(const Decl *D, StringRef Spelling) {
  iterator_range<specific_attr_iterator<AnnotateAttr>> Attrs =
      D->specific_attrs<AnnotateAttr>();

  for (AnnotateAttr *Attr : Attrs) {
    if (Attr->getAnnotation() == Spelling) {
      return true;
    }
  }

  return false;
}

inline bool isPlacementNew(const CXXNewExpr *Expression) {
  // Regular new expressions aren't placement new
  if (Expression->getNumPlacementArgs() == 0)
    return false;
  const FunctionDecl *Declaration = Expression->getOperatorNew();
  if (Declaration && hasCustomAnnotation(Declaration,
                 "moz_heap_allocator")) {
    return false;
  }
  return true;
}

#endif
