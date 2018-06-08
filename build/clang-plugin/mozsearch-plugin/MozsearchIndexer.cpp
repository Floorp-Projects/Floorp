/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <tuple>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>

#include "FileOperations.h"
#include "JSONFormatter.h"
#include "StringOperations.h"

using namespace clang;

const std::string GENERATED("__GENERATED__" PATHSEP_STRING);

// Absolute path to directory containing source code.
std::string Srcdir;

// Absolute path to objdir (including generated code).
std::string Objdir;

// Absolute path where analysis JSON output will be stored.
std::string Outdir;

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/time.h>

static double time() {
  struct timeval Tv;
  gettimeofday(&Tv, nullptr);
  return double(Tv.tv_sec) + double(Tv.tv_usec) / 1000000.;
}
#endif

// Return true if |input| is a valid C++ identifier. We don't want to generate
// analysis information for operators, string literals, etc. by accident since
// it trips up consumers of the data.
static bool isValidIdentifier(std::string Input) {
  for (char C : Input) {
    if (!(isalpha(C) || isdigit(C) || C == '_')) {
      return false;
    }
  }
  return true;
}

struct RAIITracer {
  RAIITracer(const char *log) : mLog(log) {
    printf("<%s>\n", mLog);
  }

  ~RAIITracer() {
    printf("</%s>\n", mLog);
  }

  const char* mLog;
};

#define TRACEFUNC RAIITracer tracer(__FUNCTION__);

class IndexConsumer;

// For each C++ file seen by the analysis (.cpp or .h), we track a
// FileInfo. This object tracks whether the file is "interesting" (i.e., whether
// it's in the source dir or the objdir). We also store the analysis output
// here.
struct FileInfo {
  FileInfo(std::string &Rname) : Realname(Rname) {
    if (Rname.compare(0, Objdir.length(), Objdir) == 0) {
      // We're in the objdir, so we are probably a generated header
      // We use the escape character to indicate the objdir nature.
      // Note that output also has the `/' already placed
      Interesting = true;
      Realname.replace(0, Objdir.length(), GENERATED);
      return;
    }

    // Empty filenames can get turned into Srcdir when they are resolved as
    // absolute paths, so we should exclude files that are exactly equal to
    // Srcdir or anything outside Srcdir.
    Interesting = (Rname.length() > Srcdir.length()) &&
                  (Rname.compare(0, Srcdir.length(), Srcdir) == 0);
    if (Interesting) {
      // Remove the trailing `/' as well.
      Realname.erase(0, Srcdir.length() + 1);
    }
  }
  std::string Realname;
  std::vector<std::string> Output;
  bool Interesting;
};

class IndexConsumer;

class PreprocessorHook : public PPCallbacks {
  IndexConsumer *Indexer;

public:
  PreprocessorHook(IndexConsumer *C) : Indexer(C) {}

  virtual void MacroDefined(const Token &Tok,
                            const MacroDirective *Md) override;

  virtual void MacroExpands(const Token &Tok, const MacroDefinition &Md,
                            SourceRange Range, const MacroArgs *Ma) override;
#if CLANG_VERSION_MAJOR >= 5
  virtual void MacroUndefined(const Token &Tok, const MacroDefinition &Md,
                              const MacroDirective *Undef) override;
#else
  virtual void MacroUndefined(const Token &Tok,
                              const MacroDefinition &Md) override;
#endif
  virtual void Defined(const Token &Tok, const MacroDefinition &Md,
                       SourceRange Range) override;
  virtual void Ifdef(SourceLocation Loc, const Token &Tok,
                     const MacroDefinition &Md) override;
  virtual void Ifndef(SourceLocation Loc, const Token &Tok,
                      const MacroDefinition &Md) override;
};

class IndexConsumer : public ASTConsumer,
                      public RecursiveASTVisitor<IndexConsumer>,
                      public DiagnosticConsumer {
private:
  CompilerInstance &CI;
  SourceManager &SM;
  std::map<FileID, std::unique_ptr<FileInfo>> FileMap;
  MangleContext *CurMangleContext;
  ASTContext *AstContext;

  typedef RecursiveASTVisitor<IndexConsumer> Super;

  // Tracks the set of declarations that the current expression/statement is
  // nested inside of.
  struct AutoSetContext {
    AutoSetContext(IndexConsumer *Self, NamedDecl *Context)
        : Self(Self), Prev(Self->CurDeclContext), Decl(Context) {
      Self->CurDeclContext = this;
    }

    ~AutoSetContext() { Self->CurDeclContext = Prev; }

    IndexConsumer *Self;
    AutoSetContext *Prev;
    NamedDecl *Decl;
  };
  AutoSetContext *CurDeclContext;

  FileInfo *getFileInfo(SourceLocation Loc) {
    FileID Id = SM.getFileID(Loc);

    std::map<FileID, std::unique_ptr<FileInfo>>::iterator It;
    It = FileMap.find(Id);
    if (It == FileMap.end()) {
      // We haven't seen this file before. We need to make the FileInfo
      // structure information ourselves
      std::string Filename = SM.getFilename(Loc);
      std::string Absolute;
      // If Loc is a macro id rather than a file id, it Filename might be
      // empty. Also for some types of file locations that are clang-internal
      // like "<scratch>" it can return an empty Filename. In these cases we
      // want to leave Absolute as empty.
      if (!Filename.empty()) {
        Absolute = getAbsolutePath(Filename);
        if (Absolute.empty()) {
          Absolute = Filename;
        }
      }
      std::unique_ptr<FileInfo> Info = llvm::make_unique<FileInfo>(Absolute);
      It = FileMap.insert(std::make_pair(Id, std::move(Info))).first;
    }
    return It->second.get();
  }

  // Helpers for processing declarations
  // Should we ignore this location?
  bool isInterestingLocation(SourceLocation Loc) {
    if (Loc.isInvalid()) {
      return false;
    }

    return getFileInfo(Loc)->Interesting;
  }

  std::string locationToString(SourceLocation Loc, size_t Length = 0) {
    std::pair<FileID, unsigned> Pair = SM.getDecomposedLoc(Loc);

    bool IsInvalid;
    unsigned Line = SM.getLineNumber(Pair.first, Pair.second, &IsInvalid);
    if (IsInvalid) {
      return "";
    }
    unsigned Column = SM.getColumnNumber(Pair.first, Pair.second, &IsInvalid);
    if (IsInvalid) {
      return "";
    }

    if (Length) {
      return stringFormat("%05d:%d-%d", Line, Column - 1, Column - 1 + Length);
    } else {
      return stringFormat("%05d:%d", Line, Column - 1);
    }
  }

  std::string lineRangeToString(SourceRange Range) {
    std::pair<FileID, unsigned> Begin = SM.getDecomposedLoc(Range.getBegin());
    std::pair<FileID, unsigned> End = SM.getDecomposedLoc(Range.getEnd());

    bool IsInvalid;
    unsigned Line1 = SM.getLineNumber(Begin.first, Begin.second, &IsInvalid);
    if (IsInvalid) {
      return "";
    }
    unsigned Line2 = SM.getLineNumber(End.first, End.second, &IsInvalid);
    if (IsInvalid) {
      return "";
    }

    return stringFormat("%d-%d", Line1, Line2);
  }

  // Returns the qualified name of `d` without considering template parameters.
  std::string getQualifiedName(const NamedDecl *D) {
    const DeclContext *Ctx = D->getDeclContext();
    if (Ctx->isFunctionOrMethod()) {
      return D->getQualifiedNameAsString();
    }

    std::vector<const DeclContext *> Contexts;

    // Collect contexts.
    while (Ctx && isa<NamedDecl>(Ctx)) {
      Contexts.push_back(Ctx);
      Ctx = Ctx->getParent();
    }

    std::string Result;

    std::reverse(Contexts.begin(), Contexts.end());

    for (const DeclContext *DC : Contexts) {
      if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
        Result += Spec->getNameAsString();

        if (Spec->getSpecializationKind() == TSK_ExplicitSpecialization) {
          std::string Backing;
          llvm::raw_string_ostream Stream(Backing);
          const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
#if CLANG_VERSION_MAJOR > 5
          printTemplateArgumentList(
              Stream, TemplateArgs.asArray(), PrintingPolicy(CI.getLangOpts()));
#elif CLANG_VERSION_MAJOR > 3 ||                                                 \
    (CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR >= 9)
          TemplateSpecializationType::PrintTemplateArgumentList(
              Stream, TemplateArgs.asArray(), PrintingPolicy(CI.getLangOpts()));
#else
          TemplateSpecializationType::PrintTemplateArgumentList(
              stream, templateArgs.data(), templateArgs.size(),
              PrintingPolicy(CI.getLangOpts()));
#endif
          Result += Stream.str();
        }
      } else if (const auto *Nd = dyn_cast<NamespaceDecl>(DC)) {
        if (Nd->isAnonymousNamespace() || Nd->isInline()) {
          continue;
        }
        Result += Nd->getNameAsString();
      } else if (const auto *Rd = dyn_cast<RecordDecl>(DC)) {
        if (!Rd->getIdentifier()) {
          Result += "(anonymous)";
        } else {
          Result += Rd->getNameAsString();
        }
      } else if (const auto *Fd = dyn_cast<FunctionDecl>(DC)) {
        Result += Fd->getNameAsString();
      } else if (const auto *Ed = dyn_cast<EnumDecl>(DC)) {
        // C++ [dcl.enum]p10: Each enum-name and each unscoped
        // enumerator is declared in the scope that immediately contains
        // the enum-specifier. Each scoped enumerator is declared in the
        // scope of the enumeration.
        if (Ed->isScoped() || Ed->getIdentifier())
          Result += Ed->getNameAsString();
        else
          continue;
      } else {
        Result += cast<NamedDecl>(DC)->getNameAsString();
      }
      Result += "::";
    }

    if (D->getDeclName())
      Result += D->getNameAsString();
    else
      Result += "(anonymous)";

    return Result;
  }

  std::string mangleLocation(SourceLocation Loc,
                             std::string Backup = std::string()) {
    FileInfo *F = getFileInfo(Loc);
    std::string Filename = F->Realname;
    if (Filename.length() == 0 && Backup.length() != 0) {
      return Backup;
    }
    return hash(Filename + std::string("@") + locationToString(Loc));
  }

  std::string mangleQualifiedName(std::string Name) {
    std::replace(Name.begin(), Name.end(), ' ', '_');
    return Name;
  }

  std::string getMangledName(clang::MangleContext *Ctx,
                             const clang::NamedDecl *Decl) {
    if (isa<FunctionDecl>(Decl) && cast<FunctionDecl>(Decl)->isExternC()) {
      return cast<FunctionDecl>(Decl)->getNameAsString();
    }

    if (isa<FunctionDecl>(Decl) || isa<VarDecl>(Decl)) {
      const DeclContext *DC = Decl->getDeclContext();
      if (isa<TranslationUnitDecl>(DC) || isa<NamespaceDecl>(DC) ||
          isa<LinkageSpecDecl>(DC) ||
          // isa<ExternCContextDecl>(DC) ||
          isa<TagDecl>(DC)) {
        llvm::SmallVector<char, 512> Output;
        llvm::raw_svector_ostream Out(Output);
        if (const CXXConstructorDecl *D = dyn_cast<CXXConstructorDecl>(Decl)) {
          Ctx->mangleCXXCtor(D, CXXCtorType::Ctor_Complete, Out);
        } else if (const CXXDestructorDecl *D =
                       dyn_cast<CXXDestructorDecl>(Decl)) {
          Ctx->mangleCXXDtor(D, CXXDtorType::Dtor_Complete, Out);
        } else {
          Ctx->mangleName(Decl, Out);
        }
        return Out.str().str();
      } else {
        return std::string("V_") + mangleLocation(Decl->getLocation()) +
               std::string("_") + hash(Decl->getName());
      }
    } else if (isa<TagDecl>(Decl) || isa<TypedefNameDecl>(Decl) ||
               isa<ObjCInterfaceDecl>(Decl)) {
      if (!Decl->getIdentifier()) {
        // Anonymous.
        return std::string("T_") + mangleLocation(Decl->getLocation());
      }

      return std::string("T_") + mangleQualifiedName(getQualifiedName(Decl));
    } else if (isa<NamespaceDecl>(Decl) || isa<NamespaceAliasDecl>(Decl)) {
      if (!Decl->getIdentifier()) {
        // Anonymous.
        return std::string("NS_") + mangleLocation(Decl->getLocation());
      }

      return std::string("NS_") + mangleQualifiedName(getQualifiedName(Decl));
    } else if (const ObjCIvarDecl *D2 = dyn_cast<ObjCIvarDecl>(Decl)) {
      const ObjCInterfaceDecl *Iface = D2->getContainingInterface();
      return std::string("F_<") + getMangledName(Ctx, Iface) + ">_" +
             D2->getNameAsString();
    } else if (const FieldDecl *D2 = dyn_cast<FieldDecl>(Decl)) {
      const RecordDecl *Record = D2->getParent();
      return std::string("F_<") + getMangledName(Ctx, Record) + ">_" +
             toString(D2->getFieldIndex());
    } else if (const EnumConstantDecl *D2 = dyn_cast<EnumConstantDecl>(Decl)) {
      const DeclContext *DC = Decl->getDeclContext();
      if (const NamedDecl *Named = dyn_cast<NamedDecl>(DC)) {
        return std::string("E_<") + getMangledName(Ctx, Named) + ">_" +
               D2->getNameAsString();
      }
    }

    assert(false);
    return std::string("");
  }

  void debugLocation(SourceLocation Loc) {
    std::string S = locationToString(Loc);
    StringRef Filename = SM.getFilename(Loc);
    printf("--> %s %s\n", std::string(Filename).c_str(), S.c_str());
  }

  void debugRange(SourceRange Range) {
    printf("Range\n");
    debugLocation(Range.getBegin());
    debugLocation(Range.getEnd());
  }

public:
  IndexConsumer(CompilerInstance &CI)
      : CI(CI), SM(CI.getSourceManager()), CurMangleContext(nullptr),
        AstContext(nullptr), CurDeclContext(nullptr), TemplateStack(nullptr) {
    CI.getPreprocessor().addPPCallbacks(
        llvm::make_unique<PreprocessorHook>(this));
  }

  virtual DiagnosticConsumer *clone(DiagnosticsEngine &Diags) const {
    return new IndexConsumer(CI);
  }

#if !defined(_WIN32) && !defined(_WIN64)
  struct AutoTime {
    AutoTime(double *Counter) : Counter(Counter), Start(time()) {}
    ~AutoTime() {
      if (Start) {
        *Counter += time() - Start;
      }
    }
    void stop() {
      *Counter += time() - Start;
      Start = 0;
    }
    double *Counter;
    double Start;
  };
#endif

  // All we need is to follow the final declaration.
  virtual void HandleTranslationUnit(ASTContext &Ctx) {
    CurMangleContext =
      clang::ItaniumMangleContext::create(Ctx, CI.getDiagnostics());

    AstContext = &Ctx;
    TraverseDecl(Ctx.getTranslationUnitDecl());

    // Emit the JSON data for all files now.
    std::map<FileID, std::unique_ptr<FileInfo>>::iterator It;
    for (It = FileMap.begin(); It != FileMap.end(); It++) {
      if (!It->second->Interesting) {
        continue;
      }

      FileInfo &Info = *It->second;

      std::string Filename = Outdir;
      Filename += It->second->Realname;

      ensurePath(Filename);

      // We lock the output file in case some other clang process is trying to
      // write to it at the same time.
      AutoLockFile Lock(Filename);

      if (!Lock.success()) {
        continue;
      }

      std::vector<std::string> Lines;

      // Read all the existing lines in from the output file. Rather than
      // overwrite them, we want to merge our results with what was already
      // there. This ensures that header files that are included multiple times
      // in different ways are analyzed completely.
      char Buffer[65536];
      FILE *Fp = Lock.openFile("r");
      if (!Fp) {
        fprintf(stderr, "Unable to open input file %s\n", Filename.c_str());
        exit(1);
      }
      while (fgets(Buffer, sizeof(Buffer), Fp)) {
        Lines.push_back(std::string(Buffer));
      }
      fclose(Fp);

      // Insert the newly generated analysis data into what was read. Sort the
      // results and then remove duplicates.
      Lines.insert(Lines.end(), Info.Output.begin(), Info.Output.end());
      std::sort(Lines.begin(), Lines.end());

      std::vector<std::string> Nodupes;
      std::unique_copy(Lines.begin(), Lines.end(), std::back_inserter(Nodupes));

      // Overwrite the output file with the merged data. Since we have the lock,
      // this will happen atomically.
      Fp = Lock.openFile("w");
      if (!Fp) {
        fprintf(stderr, "Unable to open output file %s\n", Filename.c_str());
        exit(1);
      }
      size_t Length = 0;
      for (std::string &Line : Nodupes) {
        Length += Line.length();
        if (fwrite(Line.c_str(), Line.length(), 1, Fp) != 1) {
          fprintf(stderr, "Unable to write to output file %s\n", Filename.c_str());
        }
      }
      fclose(Fp);

      if (!Lock.truncateFile(Length)) {
        return;
      }
    }
  }

  // Return a list of mangled names of all the methods that the given method
  // overrides.
  void findOverriddenMethods(const CXXMethodDecl *Method,
                             std::vector<std::string> &Symbols) {
    std::string Mangled = getMangledName(CurMangleContext, Method);
    Symbols.push_back(Mangled);

    CXXMethodDecl::method_iterator Iter = Method->begin_overridden_methods();
    CXXMethodDecl::method_iterator End = Method->end_overridden_methods();
    for (; Iter != End; Iter++) {
      const CXXMethodDecl *Decl = *Iter;
      if (Decl->isTemplateInstantiation()) {
        Decl = dyn_cast<CXXMethodDecl>(Decl->getTemplateInstantiationPattern());
      }
      return findOverriddenMethods(Decl, Symbols);
    }
  }

  // Unfortunately, we have to override all these methods in order to track the
  // context we're inside.

  bool TraverseEnumDecl(EnumDecl *D) {
    AutoSetContext Asc(this, D);
    return Super::TraverseEnumDecl(D);
  }
  bool TraverseRecordDecl(RecordDecl *D) {
    AutoSetContext Asc(this, D);
    return Super::TraverseRecordDecl(D);
  }
  bool TraverseCXXRecordDecl(CXXRecordDecl *D) {
    AutoSetContext Asc(this, D);
    return Super::TraverseCXXRecordDecl(D);
  }
  bool TraverseFunctionDecl(FunctionDecl *D) {
    AutoSetContext Asc(this, D);
    const FunctionDecl *Def;
    // (See the larger AutoTemplateContext comment for more information.) If a
    // method on a templated class is declared out-of-line, we need to analyze
    // the definition inside the scope of the template or else we won't properly
    // handle member access on the templated type.
    if (TemplateStack && D->isDefined(Def) && Def && D != Def) {
      TraverseFunctionDecl(const_cast<FunctionDecl *>(Def));
    }
    return Super::TraverseFunctionDecl(D);
  }
  bool TraverseCXXMethodDecl(CXXMethodDecl *D) {
    AutoSetContext Asc(this, D);
    const FunctionDecl *Def;
    // See TraverseFunctionDecl.
    if (TemplateStack && D->isDefined(Def) && Def && D != Def) {
      TraverseFunctionDecl(const_cast<FunctionDecl *>(Def));
    }
    return Super::TraverseCXXMethodDecl(D);
  }
  bool TraverseCXXConstructorDecl(CXXConstructorDecl *D) {
    AutoSetContext Asc(this, D);
    const FunctionDecl *Def;
    // See TraverseFunctionDecl.
    if (TemplateStack && D->isDefined(Def) && Def && D != Def) {
      TraverseFunctionDecl(const_cast<FunctionDecl *>(Def));
    }
    return Super::TraverseCXXConstructorDecl(D);
  }
  bool TraverseCXXConversionDecl(CXXConversionDecl *D) {
    AutoSetContext Asc(this, D);
    const FunctionDecl *Def;
    // See TraverseFunctionDecl.
    if (TemplateStack && D->isDefined(Def) && Def && D != Def) {
      TraverseFunctionDecl(const_cast<FunctionDecl *>(Def));
    }
    return Super::TraverseCXXConversionDecl(D);
  }
  bool TraverseCXXDestructorDecl(CXXDestructorDecl *D) {
    AutoSetContext Asc(this, D);
    const FunctionDecl *Def;
    // See TraverseFunctionDecl.
    if (TemplateStack && D->isDefined(Def) && Def && D != Def) {
      TraverseFunctionDecl(const_cast<FunctionDecl *>(Def));
    }
    return Super::TraverseCXXDestructorDecl(D);
  }

  // Used to keep track of the context in which a token appears.
  struct Context {
    // Ultimately this becomes the "context" JSON property.
    std::string Name;

    // Ultimately this becomes the "contextsym" JSON property.
    std::vector<std::string> Symbols;

    Context() {}
    Context(std::string Name, std::vector<std::string> Symbols)
        : Name(Name), Symbols(Symbols) {}
  };

  Context translateContext(NamedDecl *D) {
    const FunctionDecl *F = dyn_cast<FunctionDecl>(D);
    if (F && F->isTemplateInstantiation()) {
      D = F->getTemplateInstantiationPattern();
    }

    std::vector<std::string> Symbols = {getMangledName(CurMangleContext, D)};
    if (CXXMethodDecl::classof(D)) {
      Symbols.clear();
      findOverriddenMethods(dyn_cast<CXXMethodDecl>(D), Symbols);
    }
    return Context(D->getQualifiedNameAsString(), Symbols);
  }

  Context getContext(SourceLocation Loc) {
    if (SM.isMacroBodyExpansion(Loc)) {
      // If we're inside a macro definition, we don't return any context. It
      // will probably not be what the user expects if we do.
      return Context();
    }

    if (CurDeclContext) {
      return translateContext(CurDeclContext->Decl);
    }
    return Context();
  }

  // Similar to GetContext(SourceLocation), but it skips the declaration passed
  // in. This is useful if we want the context of a declaration that's already
  // on the stack.
  Context getContext(Decl *D) {
    if (SM.isMacroBodyExpansion(D->getLocation())) {
      // If we're inside a macro definition, we don't return any context. It
      // will probably not be what the user expects if we do.
      return Context();
    }

    AutoSetContext *Ctxt = CurDeclContext;
    while (Ctxt) {
      if (Ctxt->Decl != D) {
        return translateContext(Ctxt->Decl);
      }
      Ctxt = Ctxt->Prev;
    }
    return Context();
  }

  static std::string concatSymbols(const std::vector<std::string> Symbols) {
    if (Symbols.empty()) {
      return "";
    }

    size_t Total = 0;
    for (auto It = Symbols.begin(); It != Symbols.end(); It++) {
      Total += It->length();
    }
    Total += Symbols.size() - 1;

    std::string SymbolList;
    SymbolList.reserve(Total);

    for (auto It = Symbols.begin(); It != Symbols.end(); It++) {
      std::string Symbol = *It;

      if (It != Symbols.begin()) {
        SymbolList.push_back(',');
      }
      SymbolList.append(Symbol);
    }

    return SymbolList;
  }

  // Analyzing template code is tricky. Suppose we have this code:
  //
  //   template<class T>
  //   bool Foo(T* ptr) { return T::StaticMethod(ptr); }
  //
  // If we analyze the body of Foo without knowing the type T, then we will not
  // be able to generate any information for StaticMethod. However, analyzing
  // Foo for every possible instantiation is inefficient and it also generates
  // too much data in some cases. For example, the following code would generate
  // one definition of Baz for every instantiation, which is undesirable:
  //
  //   template<class T>
  //   class Bar { struct Baz { ... }; };
  //
  // To solve this problem, we analyze templates only once. We do so in a
  // GatherDependent mode where we look for "dependent scoped member
  // expressions" (i.e., things like StaticMethod). We keep track of the
  // locations of these expressions. If we find one or more of them, we analyze
  // the template for each instantiation, in an AnalyzeDependent mode. This mode
  // ignores all source locations except for the ones where we found dependent
  // scoped member expressions before. For these locations, we generate a
  // separate JSON result for each instantiation.
  struct AutoTemplateContext {
    AutoTemplateContext(IndexConsumer *Self)
        : Self(Self), CurMode(Mode::GatherDependent),
          Parent(Self->TemplateStack) {
      Self->TemplateStack = this;
    }

    ~AutoTemplateContext() { Self->TemplateStack = Parent; }

    // We traverse templates in two modes:
    enum class Mode {
      // Gather mode does not traverse into specializations. It looks for
      // locations where it would help to have more info from template
      // specializations.
      GatherDependent,

      // Analyze mode traverses into template specializations and records
      // information about token locations saved in gather mode.
      AnalyzeDependent,
    };

    // We found a dependent scoped member expression! Keep track of it for
    // later.
    void visitDependent(SourceLocation Loc) {
      if (CurMode == Mode::AnalyzeDependent) {
        return;
      }

      DependentLocations.insert(Loc.getRawEncoding());
      if (Parent) {
        Parent->visitDependent(Loc);
      }
    }

    // Do we need to perform the extra AnalyzeDependent passes (one per
    // instantiation)?
    bool needsAnalysis() const {
      if (!DependentLocations.empty()) {
        return true;
      }
      if (Parent) {
        return Parent->needsAnalysis();
      }
      return false;
    }

    void switchMode() { CurMode = Mode::AnalyzeDependent; }

    // Do we want to analyze each template instantiation separately?
    bool shouldVisitTemplateInstantiations() const {
      if (CurMode == Mode::AnalyzeDependent) {
        return true;
      }
      if (Parent) {
        return Parent->shouldVisitTemplateInstantiations();
      }
      return false;
    }

    // For a given expression/statement, should we emit JSON data for it?
    bool shouldVisit(SourceLocation Loc) {
      if (CurMode == Mode::GatherDependent) {
        return true;
      }
      if (DependentLocations.find(Loc.getRawEncoding()) !=
          DependentLocations.end()) {
        return true;
      }
      if (Parent) {
        return Parent->shouldVisit(Loc);
      }
      return false;
    }

  private:
    IndexConsumer *Self;
    Mode CurMode;
    std::unordered_set<unsigned> DependentLocations;
    AutoTemplateContext *Parent;
  };

  AutoTemplateContext *TemplateStack;

  bool shouldVisitTemplateInstantiations() const {
    if (TemplateStack) {
      return TemplateStack->shouldVisitTemplateInstantiations();
    }
    return false;
  }

  bool TraverseClassTemplateDecl(ClassTemplateDecl *D) {
    AutoTemplateContext Atc(this);
    Super::TraverseClassTemplateDecl(D);

    if (!Atc.needsAnalysis()) {
      return true;
    }

    Atc.switchMode();

    if (D != D->getCanonicalDecl()) {
      return true;
    }

    for (auto *Spec : D->specializations()) {
      for (auto *Rd : Spec->redecls()) {
        // We don't want to visit injected-class-names in this traversal.
        if (cast<CXXRecordDecl>(Rd)->isInjectedClassName())
          continue;

        TraverseDecl(Rd);
      }
    }

    return true;
  }

  bool TraverseFunctionTemplateDecl(FunctionTemplateDecl *D) {
    AutoTemplateContext Atc(this);
    Super::TraverseFunctionTemplateDecl(D);

    if (!Atc.needsAnalysis()) {
      return true;
    }

    Atc.switchMode();

    if (D != D->getCanonicalDecl()) {
      return true;
    }

    for (auto *Spec : D->specializations()) {
      for (auto *Rd : Spec->redecls()) {
        TraverseDecl(Rd);
      }
    }

    return true;
  }

  bool shouldVisit(SourceLocation Loc) {
    if (TemplateStack) {
      return TemplateStack->shouldVisit(Loc);
    }
    return true;
  }

  enum {
    NoCrossref = 1 << 0,
    OperatorToken = 1 << 1,
  };

  // This is the only function that emits analysis JSON data. It should be
  // called for each identifier that corresponds to a symbol.
  void visitIdentifier(const char *Kind, const char *SyntaxKind,
                       std::string QualName, SourceLocation Loc,
                       const std::vector<std::string> &Symbols,
                       Context TokenContext = Context(), int Flags = 0,
                       SourceRange PeekRange = SourceRange()) {
    if (!shouldVisit(Loc)) {
      return;
    }

    // Find the file positions corresponding to the token.
    unsigned StartOffset = SM.getFileOffset(Loc);
    unsigned EndOffset =
        StartOffset + Lexer::MeasureTokenLength(Loc, SM, CI.getLangOpts());

    std::string LocStr = locationToString(Loc, EndOffset - StartOffset);
    std::string RangeStr = locationToString(Loc, EndOffset - StartOffset);
    std::string PeekRangeStr;

    if (!(Flags & OperatorToken)) {
      // Get the token's characters so we can make sure it's a valid token.
      const char *StartChars = SM.getCharacterData(Loc);
      std::string Text(StartChars, EndOffset - StartOffset);
      if (!isValidIdentifier(Text)) {
        return;
      }
    }

    FileInfo *F = getFileInfo(Loc);

    std::string SymbolList;

    // Reserve space in symbolList for everything in `symbols`. `symbols` can
    // contain some very long strings.
    size_t Total = 0;
    for (auto It = Symbols.begin(); It != Symbols.end(); It++) {
      Total += It->length();
    }

    // Space for commas.
    Total += Symbols.size() - 1;
    SymbolList.reserve(Total);

    // For each symbol, generate one "target":1 item. We want to find this line
    // if someone searches for any one of these symbols.
    for (auto It = Symbols.begin(); It != Symbols.end(); It++) {
      std::string Symbol = *It;

      if (!(Flags & NoCrossref)) {
        JSONFormatter Fmt;

        Fmt.add("loc", LocStr);
        Fmt.add("target", 1);
        Fmt.add("kind", Kind);
        Fmt.add("pretty", QualName);
        Fmt.add("sym", Symbol);
        if (!TokenContext.Name.empty()) {
          Fmt.add("context", TokenContext.Name);
        }
        std::string ContextSymbol = concatSymbols(TokenContext.Symbols);
        if (!ContextSymbol.empty()) {
          Fmt.add("contextsym", ContextSymbol);
        }
        if (PeekRange.isValid()) {
          PeekRangeStr = lineRangeToString(PeekRange);
          if (!PeekRangeStr.empty()) {
            Fmt.add("peekRange", PeekRangeStr);
          }
        }

        std::string S;
        Fmt.format(S);
        F->Output.push_back(std::move(S));
      }

      if (It != Symbols.begin()) {
        SymbolList.push_back(',');
      }
      SymbolList.append(Symbol);
    }

    // Generate a single "source":1 for all the symbols. If we search from here,
    // we want to union the results for every symbol in `symbols`.
    JSONFormatter Fmt;

    Fmt.add("loc", RangeStr);
    Fmt.add("source", 1);

    std::string Syntax;
    if (Flags & NoCrossref) {
      Fmt.add("syntax", "");
    } else {
      Syntax = Kind;
      Syntax.push_back(',');
      Syntax.append(SyntaxKind);
      Fmt.add("syntax", Syntax);
    }

    std::string Pretty(SyntaxKind);
    Pretty.push_back(' ');
    Pretty.append(QualName);
    Fmt.add("pretty", Pretty);

    Fmt.add("sym", SymbolList);

    if (Flags & NoCrossref) {
      Fmt.add("no_crossref", 1);
    }

    std::string Buf;
    Fmt.format(Buf);
    F->Output.push_back(std::move(Buf));
  }

  void visitIdentifier(const char *Kind, const char *SyntaxKind,
                       std::string QualName, SourceLocation Loc, std::string Symbol,
                       Context TokenContext = Context(), int Flags = 0,
                       SourceRange PeekRange = SourceRange()) {
    std::vector<std::string> V = {Symbol};
    visitIdentifier(Kind, SyntaxKind, QualName, Loc, V, TokenContext, Flags, PeekRange);
  }

  void normalizeLocation(SourceLocation *Loc) {
    *Loc = SM.getSpellingLoc(*Loc);
  }

  SourceRange getFunctionPeekRange(FunctionDecl* D) {
    // We always start at the start of the function decl, which may include the
    // return type on a separate line.
    SourceLocation Start = D->getLocStart();

    // By default, we end at the line containing the function's name.
    SourceLocation End = D->getLocation();

    std::pair<FileID, unsigned> FuncLoc = SM.getDecomposedLoc(End);

    // But if there are parameters, we want to include those as well.
    for (ParmVarDecl* Param : D->parameters()) {
      std::pair<FileID, unsigned> ParamLoc = SM.getDecomposedLoc(Param->getLocation());

      // It's possible there are macros involved or something. We don't include
      // the parameters in that case.
      if (ParamLoc.first == FuncLoc.first) {
        // Assume parameters are in order, so we always take the last one.
        End = Param->getLocEnd();
      }
    }

    return SourceRange(Start, End);
  }

  SourceRange getTagPeekRange(TagDecl* D) {
    SourceLocation Start = D->getLocStart();

    // By default, we end at the line containing the name.
    SourceLocation End = D->getLocation();

    std::pair<FileID, unsigned> FuncLoc = SM.getDecomposedLoc(End);

    if (CXXRecordDecl* D2 = dyn_cast<CXXRecordDecl>(D)) {
      // But if there are parameters, we want to include those as well.
      for (CXXBaseSpecifier& Base : D2->bases()) {
        std::pair<FileID, unsigned> Loc = SM.getDecomposedLoc(Base.getLocEnd());

        // It's possible there are macros involved or something. We don't include
        // the parameters in that case.
        if (Loc.first == FuncLoc.first) {
          // Assume parameters are in order, so we always take the last one.
          End = Base.getLocEnd();
        }
      }
    }

    return SourceRange(Start, End);
  }

  SourceRange getCommentRange(NamedDecl* D) {
    const RawComment* RC =
      AstContext->getRawCommentForDeclNoCache(D);
    if (!RC) {
      return SourceRange();
    }

    return RC->getSourceRange();
  }

  SourceRange combineRanges(SourceRange Range1, SourceRange Range2) {
    if (Range1.isInvalid()) {
      return Range2;
    }
    if (Range2.isInvalid()) {
      return Range1;
    }

    std::pair<FileID, unsigned> Begin1 = SM.getDecomposedLoc(Range1.getBegin());
    std::pair<FileID, unsigned> End1 = SM.getDecomposedLoc(Range1.getEnd());
    std::pair<FileID, unsigned> Begin2 = SM.getDecomposedLoc(Range2.getBegin());
    std::pair<FileID, unsigned> End2 = SM.getDecomposedLoc(Range2.getEnd());

    if (End1.first != Begin2.first) {
      // Something weird is probably happening with the preprocessor. Just
      // return the first range.
      return Range1;
    }

    // See which range comes first.
    if (Begin1.second <= End2.second) {
      return SourceRange(Range1.getBegin(), Range2.getEnd());
    } else {
      return SourceRange(Range2.getBegin(), Range1.getEnd());
    }
  }

  SourceRange validateRange(SourceLocation Loc, SourceRange Range) {
    std::pair<FileID, unsigned> Decomposed = SM.getDecomposedLoc(Loc);
    std::pair<FileID, unsigned> Begin = SM.getDecomposedLoc(Range.getBegin());
    std::pair<FileID, unsigned> End = SM.getDecomposedLoc(Range.getEnd());

    if (Begin.first != Decomposed.first || End.first != Decomposed.first) {
      return SourceRange();
    }

    if (Begin.second >= End.second) {
      return SourceRange();
    }

    return Range;
  }

  bool VisitNamedDecl(NamedDecl *D) {
    SourceLocation Loc = D->getLocation();

    // If the token is from a macro expansion and the expansion location
    // is interesting, use that instead as it tends to be more useful.
    if (SM.isMacroBodyExpansion(Loc)) {
      Loc = SM.getFileLoc(Loc);
    }

    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    if (isa<ParmVarDecl>(D) && !D->getDeclName().getAsIdentifierInfo()) {
      // Unnamed parameter in function proto.
      return true;
    }

    int Flags = 0;
    const char *Kind = "def";
    const char *PrettyKind = "?";
    SourceRange PeekRange(D->getLocStart(), D->getLocEnd());
    if (FunctionDecl *D2 = dyn_cast<FunctionDecl>(D)) {
      if (D2->isTemplateInstantiation()) {
        D = D2->getTemplateInstantiationPattern();
      }
      Kind = D2->isThisDeclarationADefinition() ? "def" : "decl";
      PrettyKind = "function";
      PeekRange = getFunctionPeekRange(D2);
    } else if (TagDecl *D2 = dyn_cast<TagDecl>(D)) {
      Kind = D2->isThisDeclarationADefinition() ? "def" : "decl";
      PrettyKind = "type";

      if (D2->isThisDeclarationADefinition() && D2->getDefinition() == D2) {
        PeekRange = getTagPeekRange(D2);
      } else {
        PeekRange = SourceRange();
      }
    } else if (isa<TypedefNameDecl>(D)) {
      Kind = "def";
      PrettyKind = "type";
      PeekRange = SourceRange(Loc, Loc);
    } else if (VarDecl *D2 = dyn_cast<VarDecl>(D)) {
      if (D2->isLocalVarDeclOrParm()) {
        Flags = NoCrossref;
      }

      Kind = D2->isThisDeclarationADefinition() == VarDecl::DeclarationOnly
                 ? "decl"
                 : "def";
      PrettyKind = "variable";
    } else if (isa<NamespaceDecl>(D) || isa<NamespaceAliasDecl>(D)) {
      Kind = "def";
      PrettyKind = "namespace";
      PeekRange = SourceRange(Loc, Loc);
    } else if (isa<FieldDecl>(D)) {
      Kind = "def";
      PrettyKind = "field";
    } else if (isa<EnumConstantDecl>(D)) {
      Kind = "def";
      PrettyKind = "enum constant";
    } else {
      return true;
    }

    SourceRange CommentRange = getCommentRange(D);
    PeekRange = combineRanges(PeekRange, CommentRange);
    PeekRange = validateRange(Loc, PeekRange);

    std::vector<std::string> Symbols = {getMangledName(CurMangleContext, D)};
    if (CXXMethodDecl::classof(D)) {
      Symbols.clear();
      findOverriddenMethods(dyn_cast<CXXMethodDecl>(D), Symbols);
    }

    // For destructors, loc points to the ~ character. We want to skip to the
    // class name.
    if (isa<CXXDestructorDecl>(D)) {
      const char *P = SM.getCharacterData(Loc);
      assert(*p == '~');
      P++;

      unsigned Skipped = 1;
      while (*P == ' ' || *P == '\t' || *P == '\r' || *P == '\n') {
        P++;
        Skipped++;
      }

      Loc = Loc.getLocWithOffset(Skipped);

      PrettyKind = "destructor";
    }

    visitIdentifier(Kind, PrettyKind, getQualifiedName(D), Loc, Symbols,
                    getContext(D), Flags, PeekRange);

    return true;
  }

  bool VisitCXXConstructExpr(CXXConstructExpr *E) {
    SourceLocation Loc = E->getLocStart();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    FunctionDecl *Ctor = E->getConstructor();
    if (Ctor->isTemplateInstantiation()) {
      Ctor = Ctor->getTemplateInstantiationPattern();
    }
    std::string Mangled = getMangledName(CurMangleContext, Ctor);

    // FIXME: Need to do something different for list initialization.

    visitIdentifier("use", "constructor", getQualifiedName(Ctor), Loc, Mangled,
                    getContext(Loc));

    return true;
  }

  bool VisitCallExpr(CallExpr *E) {
    Decl *Callee = E->getCalleeDecl();
    if (!Callee || !FunctionDecl::classof(Callee)) {
      return true;
    }

    const NamedDecl *NamedCallee = dyn_cast<NamedDecl>(Callee);

    SourceLocation Loc;

    const FunctionDecl *F = dyn_cast<FunctionDecl>(NamedCallee);
    if (F->isTemplateInstantiation()) {
      NamedCallee = F->getTemplateInstantiationPattern();
    }

    std::string Mangled = getMangledName(CurMangleContext, NamedCallee);
    int Flags = 0;

    Expr *CalleeExpr = E->getCallee()->IgnoreParenImpCasts();

    if (CXXOperatorCallExpr::classof(E)) {
      // Just take the first token.
      CXXOperatorCallExpr *Op = dyn_cast<CXXOperatorCallExpr>(E);
      Loc = Op->getOperatorLoc();
      Flags |= OperatorToken;
    } else if (MemberExpr::classof(CalleeExpr)) {
      MemberExpr *Member = dyn_cast<MemberExpr>(CalleeExpr);
      Loc = Member->getMemberLoc();
    } else if (DeclRefExpr::classof(CalleeExpr)) {
      // We handle this in VisitDeclRefExpr.
      return true;
    } else {
      return true;
    }

    normalizeLocation(&Loc);

    if (!isInterestingLocation(Loc)) {
      return true;
    }

    visitIdentifier("use", "function", getQualifiedName(NamedCallee), Loc, Mangled,
                    getContext(Loc), Flags);

    return true;
  }

  bool VisitTagTypeLoc(TagTypeLoc L) {
    SourceLocation Loc = L.getBeginLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    TagDecl *Decl = L.getDecl();
    std::string Mangled = getMangledName(CurMangleContext, Decl);
    visitIdentifier("use", "type", getQualifiedName(Decl), Loc, Mangled,
                    getContext(Loc));
    return true;
  }

  bool VisitTypedefTypeLoc(TypedefTypeLoc L) {
    SourceLocation Loc = L.getBeginLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    NamedDecl *Decl = L.getTypedefNameDecl();
    std::string Mangled = getMangledName(CurMangleContext, Decl);
    visitIdentifier("use", "type", getQualifiedName(Decl), Loc, Mangled,
                    getContext(Loc));
    return true;
  }

  bool VisitInjectedClassNameTypeLoc(InjectedClassNameTypeLoc L) {
    SourceLocation Loc = L.getBeginLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    NamedDecl *Decl = L.getDecl();
    std::string Mangled = getMangledName(CurMangleContext, Decl);
    visitIdentifier("use", "type", getQualifiedName(Decl), Loc, Mangled,
                    getContext(Loc));
    return true;
  }

  bool VisitTemplateSpecializationTypeLoc(TemplateSpecializationTypeLoc L) {
    SourceLocation Loc = L.getBeginLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    TemplateDecl *Td = L.getTypePtr()->getTemplateName().getAsTemplateDecl();
    if (ClassTemplateDecl *D = dyn_cast<ClassTemplateDecl>(Td)) {
      NamedDecl *Decl = D->getTemplatedDecl();
      std::string Mangled = getMangledName(CurMangleContext, Decl);
      visitIdentifier("use", "type", getQualifiedName(Decl), Loc, Mangled,
                      getContext(Loc));
    } else if (TypeAliasTemplateDecl *D = dyn_cast<TypeAliasTemplateDecl>(Td)) {
      NamedDecl *Decl = D->getTemplatedDecl();
      std::string Mangled = getMangledName(CurMangleContext, Decl);
      visitIdentifier("use", "type", getQualifiedName(Decl), Loc, Mangled,
                      getContext(Loc));
    }

    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *E) {
    SourceLocation Loc = E->getExprLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    if (E->hasQualifier()) {
      Loc = E->getNameInfo().getLoc();
      normalizeLocation(&Loc);
    }

    NamedDecl *Decl = E->getDecl();
    if (const VarDecl *D2 = dyn_cast<VarDecl>(Decl)) {
      int Flags = 0;
      if (D2->isLocalVarDeclOrParm()) {
        Flags = NoCrossref;
      }
      std::string Mangled = getMangledName(CurMangleContext, Decl);
      visitIdentifier("use", "variable", getQualifiedName(Decl), Loc, Mangled,
                      getContext(Loc), Flags);
    } else if (isa<FunctionDecl>(Decl)) {
      const FunctionDecl *F = dyn_cast<FunctionDecl>(Decl);
      if (F->isTemplateInstantiation()) {
        Decl = F->getTemplateInstantiationPattern();
      }

      std::string Mangled = getMangledName(CurMangleContext, Decl);
      visitIdentifier("use", "function", getQualifiedName(Decl), Loc, Mangled,
                      getContext(Loc));
    } else if (isa<EnumConstantDecl>(Decl)) {
      std::string Mangled = getMangledName(CurMangleContext, Decl);
      visitIdentifier("use", "enum", getQualifiedName(Decl), Loc, Mangled,
                      getContext(Loc));
    }

    return true;
  }

  bool VisitCXXConstructorDecl(CXXConstructorDecl *D) {
    if (!isInterestingLocation(D->getLocation())) {
      return true;
    }

    for (CXXConstructorDecl::init_const_iterator It = D->init_begin();
         It != D->init_end(); ++It) {
      const CXXCtorInitializer *Ci = *It;
      if (!Ci->getMember() || !Ci->isWritten()) {
        continue;
      }

      SourceLocation Loc = Ci->getMemberLocation();
      normalizeLocation(&Loc);
      if (!isInterestingLocation(Loc)) {
        continue;
      }

      FieldDecl *Member = Ci->getMember();
      std::string Mangled = getMangledName(CurMangleContext, Member);
      visitIdentifier("use", "field", getQualifiedName(Member), Loc, Mangled,
                      getContext(D));
    }

    return true;
  }

  bool VisitMemberExpr(MemberExpr *E) {
    SourceLocation Loc = E->getExprLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    ValueDecl *Decl = E->getMemberDecl();
    if (FieldDecl *Field = dyn_cast<FieldDecl>(Decl)) {
      std::string Mangled = getMangledName(CurMangleContext, Field);
      visitIdentifier("use", "field", getQualifiedName(Field), Loc, Mangled,
                      getContext(Loc));
    }
    return true;
  }

  bool VisitCXXDependentScopeMemberExpr(CXXDependentScopeMemberExpr *E) {
    SourceLocation Loc = E->getMemberLoc();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return true;
    }

    if (TemplateStack) {
      TemplateStack->visitDependent(Loc);
    }
    return true;
  }

  void macroDefined(const Token &Tok, const MacroDirective *Macro) {
    if (Macro->getMacroInfo()->isBuiltinMacro()) {
      return;
    }
    SourceLocation Loc = Tok.getLocation();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return;
    }

    IdentifierInfo *Ident = Tok.getIdentifierInfo();
    if (Ident) {
      std::string Mangled =
          std::string("M_") + mangleLocation(Loc, Ident->getName());
      visitIdentifier("def", "macro", Ident->getName(), Loc, Mangled);
    }
  }

  void macroUsed(const Token &Tok, const MacroInfo *Macro) {
    if (!Macro) {
      return;
    }
    if (Macro->isBuiltinMacro()) {
      return;
    }
    SourceLocation Loc = Tok.getLocation();
    normalizeLocation(&Loc);
    if (!isInterestingLocation(Loc)) {
      return;
    }

    IdentifierInfo *Ident = Tok.getIdentifierInfo();
    if (Ident) {
      std::string Mangled =
          std::string("M_") +
          mangleLocation(Macro->getDefinitionLoc(), Ident->getName());
      visitIdentifier("use", "macro", Ident->getName(), Loc, Mangled);
    }
  }
};

void PreprocessorHook::MacroDefined(const Token &Tok,
                                    const MacroDirective *Md) {
  Indexer->macroDefined(Tok, Md);
}

void PreprocessorHook::MacroExpands(const Token &Tok, const MacroDefinition &Md,
                                    SourceRange Range, const MacroArgs *Ma) {
  Indexer->macroUsed(Tok, Md.getMacroInfo());
}

#if CLANG_VERSION_MAJOR >= 5
void PreprocessorHook::MacroUndefined(const Token &Tok,
                                      const MacroDefinition &Md,
                                      const MacroDirective *Undef)
#else
void PreprocessorHook::MacroUndefined(const Token &Tok,
                                      const MacroDefinition &Md)
#endif
{
  Indexer->macroUsed(Tok, Md.getMacroInfo());
}

void PreprocessorHook::Defined(const Token &Tok, const MacroDefinition &Md,
                               SourceRange Range) {
  Indexer->macroUsed(Tok, Md.getMacroInfo());
}

void PreprocessorHook::Ifdef(SourceLocation Loc, const Token &Tok,
                             const MacroDefinition &Md) {
  Indexer->macroUsed(Tok, Md.getMacroInfo());
}

void PreprocessorHook::Ifndef(SourceLocation Loc, const Token &Tok,
                              const MacroDefinition &Md) {
  Indexer->macroUsed(Tok, Md.getMacroInfo());
}

class IndexAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef F) {
    return llvm::make_unique<IndexConsumer>(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &Args) {
    if (Args.size() != 3) {
      DiagnosticsEngine &D = CI.getDiagnostics();
      unsigned DiagID = D.getCustomDiagID(
          DiagnosticsEngine::Error,
          "Need arguments for the source, output, and object directories");
      D.Report(DiagID);
      return false;
    }

    // Load our directories
    Srcdir = getAbsolutePath(Args[0]);
    if (Srcdir.empty()) {
      DiagnosticsEngine &D = CI.getDiagnostics();
      unsigned DiagID = D.getCustomDiagID(
          DiagnosticsEngine::Error, "Source directory '%0' does not exist");
      D.Report(DiagID) << Args[0];
      return false;
    }

    ensurePath(Args[1] + PATHSEP_STRING);
    Outdir = getAbsolutePath(Args[1]);
    Outdir += PATHSEP_STRING;

    Objdir = getAbsolutePath(Args[2]);
    if (Objdir.empty()) {
      DiagnosticsEngine &D = CI.getDiagnostics();
      unsigned DiagID = D.getCustomDiagID(DiagnosticsEngine::Error,
                                          "Objdir '%0' does not exist");
      D.Report(DiagID) << Args[2];
      return false;
    }
    Objdir += PATHSEP_STRING;

    printf("MOZSEARCH: %s %s %s\n", Srcdir.c_str(), Outdir.c_str(),
           Objdir.c_str());

    return true;
  }

  void printHelp(llvm::raw_ostream &Ros) {
    Ros << "Help for mozsearch plugin goes here\n";
  }
};

static FrontendPluginRegistry::Add<IndexAction>
    Y("mozsearch-index", "create the mozsearch index database");
