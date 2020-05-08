/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS parser. */

#ifndef frontend_Parser_h
#define frontend_Parser_h

/*
 * [SMDOC] JS Parser
 *
 * JS parsers capable of generating ASTs from source text.
 *
 * A parser embeds token stream information, then gets and matches tokens to
 * generate a syntax tree that, if desired, BytecodeEmitter will use to compile
 * bytecode.
 *
 * Like token streams (see the comment near the top of TokenStream.h), parser
 * classes are heavily templatized -- along the token stream's character-type
 * axis, and also along a full-parse/syntax-parse axis.  Certain limitations of
 * C++ (primarily the inability to partially specialize function templates),
 * plus the desire to minimize compiled code size in duplicate function
 * template instantiations wherever possible, mean that Parser exhibits much of
 * the same unholy template/inheritance complexity as token streams.
 *
 * == ParserSharedBase → JS::AutoGCRooter ==
 *
 * ParserSharedBase is the base class for both regular JS and BinAST parser.
 * This class contains common fields and methods between both parsers.
 *
 * Of particular note: making ParserSharedBase inherit JS::AutoGCRooter (rather
 * than placing it under one of the more-derived parser classes) means that all
 * parsers can be traced using the same AutoGCRooter mechanism: it's not
 * necessary to have separate tracing functionality for syntax/full parsers or
 * parsers of different character types.
 *
 * == ParserBase → ParserSharedBase, ErrorReportMixin ==
 *
 * ParserBase is the base class for regular JS parser, shared by all regular JS
 * parsers of all character types and parse-handling behavior.  It stores
 * everything character- and handler-agnostic.
 *
 * ParserBase's most important field is the parser's token stream's
 * |TokenStreamAnyChars| component, for all tokenizing aspects that are
 * character-type-agnostic.  The character-type-sensitive components residing
 * in |TokenStreamSpecific| (see the comment near the top of TokenStream.h)
 * live elsewhere in this hierarchy.  These separate locations are the reason
 * for the |AnyCharsAccess| template parameter to |TokenStreamChars| and
 * |TokenStreamSpecific|.
 *
 * == PerHandlerParser<ParseHandler> → ParserBase ==
 *
 * Certain parsing behavior varies between full parsing and syntax-only parsing
 * but does not vary across source-text character types.  For example, the work
 * to "create an arguments object for a function" obviously varies between
 * syntax and full parsing but (because no source characters are examined) does
 * not vary by source text character type.  Such functionality is implemented
 * through functions in PerHandlerParser.
 *
 * Functionality only used by syntax parsing or full parsing doesn't live here:
 * it should be implemented in the appropriate Parser<ParseHandler> (described
 * further below).
 *
 * == GeneralParser<ParseHandler, Unit> → PerHandlerParser<ParseHandler> ==
 *
 * Most parsing behavior varies across the character-type axis (and possibly
 * along the full/syntax axis).  For example:
 *
 *   * Parsing ECMAScript's Expression production, implemented by
 *     GeneralParser::expr, varies in this manner: different types are used to
 *     represent nodes in full and syntax parsing (ParseNode* versus an enum),
 *     and reading the tokens comprising the expression requires inspecting
 *     individual characters (necessarily dependent upon character type).
 *   * Reporting an error or warning does not depend on the full/syntax parsing
 *     distinction.  But error reports and warnings include a line of context
 *     (or a slice of one), for pointing out where a mistake was made.
 *     Computing such line of context requires inspecting the source text to
 *     make that line/slice of context, which requires knowing the source text
 *     character type.
 *
 * Such functionality, implemented using identical function code across these
 * axes, should live in GeneralParser.
 *
 * GeneralParser's most important field is the parser's token stream's
 * |TokenStreamSpecific| component, for all aspects of tokenizing that (contra
 * |TokenStreamAnyChars| in ParserBase above) are character-type-sensitive.  As
 * noted above, this field's existence separate from that in ParserBase
 * motivates the |AnyCharsAccess| template parameters on various token stream
 * classes.
 *
 * Everything in PerHandlerParser *could* be folded into GeneralParser (below)
 * if desired.  We don't fold in this manner because all such functions would
 * be instantiated once per Unit -- but if exactly equivalent code would be
 * generated (because PerHandlerParser functions have no awareness of Unit),
 * it's risky to *depend* upon the compiler coalescing the instantiations into
 * one in the final binary.  PerHandlerParser guarantees no duplication.
 *
 * == Parser<ParseHandler, Unit> final → GeneralParser<ParseHandler, Unit> ==
 *
 * The final (pun intended) axis of complexity lies in Parser.
 *
 * Some functionality depends on character type, yet also is defined in
 * significantly different form in full and syntax parsing.  For example,
 * attempting to parse the source text of a module will do so in full parsing
 * but immediately fail in syntax parsing -- so the former is a mess'o'code
 * while the latter is effectively |return null();|.  Such functionality is
 * defined in Parser<SyntaxParseHandler or FullParseHandler, Unit> as
 * appropriate.
 *
 * There's a crucial distinction between GeneralParser and Parser, that
 * explains why both must exist (despite taking exactly the same template
 * parameters, and despite GeneralParser and Parser existing in a one-to-one
 * relationship).  GeneralParser is one unspecialized template class:
 *
 *   template<class ParseHandler, typename Unit>
 *   class GeneralParser : ...
 *   {
 *     ...parsing functions...
 *   };
 *
 * but Parser is one undefined template class with two separate
 * specializations:
 *
 *   // Declare, but do not define.
 *   template<class ParseHandler, typename Unit> class Parser;
 *
 *   // Define a syntax-parsing specialization.
 *   template<typename Unit>
 *   class Parser<SyntaxParseHandler, Unit> final
 *     : public GeneralParser<SyntaxParseHandler, Unit>
 *   {
 *     ...parsing functions...
 *   };
 *
 *   // Define a full-parsing specialization.
 *   template<typename Unit>
 *   class Parser<SyntaxParseHandler, Unit> final
 *     : public GeneralParser<SyntaxParseHandler, Unit>
 *   {
 *     ...parsing functions...
 *   };
 *
 * This odd distinction is necessary because C++ unfortunately doesn't allow
 * partial function specialization:
 *
 *   // BAD: You can only specialize a template function if you specify *every*
 *   //      template parameter, i.e. ParseHandler *and* Unit.
 *   template<typename Unit>
 *   void
 *   GeneralParser<SyntaxParseHandler, Unit>::foo() {}
 *
 * But if you specialize Parser *as a class*, then this is allowed:
 *
 *   template<typename Unit>
 *   void
 *   Parser<SyntaxParseHandler, Unit>::foo() {}
 *
 *   template<typename Unit>
 *   void
 *   Parser<FullParseHandler, Unit>::foo() {}
 *
 * because the only template parameter on the function is Unit -- and so all
 * template parameters *are* varying, not a strict subset of them.
 *
 * So -- any parsing functionality that is differently defined for different
 * ParseHandlers, *but* is defined textually identically for different Unit
 * (even if different code ends up generated for them by the compiler), should
 * reside in Parser.
 */

#include "mozilla/Array.h"
#include "mozilla/Maybe.h"

#include <type_traits>
#include <utility>

#include "jspubtd.h"

#include "ds/Nestable.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/CompilationInfo.h"
#include "frontend/ErrorReporter.h"
#include "frontend/FullParseHandler.h"
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/NameAnalysisTypes.h"
#include "frontend/NameCollections.h"
#include "frontend/ParseContext.h"
#include "frontend/SharedContext.h"
#include "frontend/SyntaxParseHandler.h"
#include "frontend/TokenStream.h"
#include "js/Vector.h"
#include "vm/ErrorReporting.h"
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind

namespace js {

class ModuleObject;

namespace frontend {

template <class ParseHandler, typename Unit>
class GeneralParser;

class SourceParseContext : public ParseContext {
 public:
  template <typename ParseHandler, typename Unit>
  SourceParseContext(GeneralParser<ParseHandler, Unit>* prs, SharedContext* sc,
                     Directives* newDirectives)
      : ParseContext(prs->cx_, prs->pc_, sc, prs->tokenStream,
                     prs->getCompilationInfo(), newDirectives,
                     std::is_same_v<ParseHandler, FullParseHandler>) {}
};

enum VarContext { HoistVars, DontHoistVars };
enum PropListType { ObjectLiteral, ClassBody, DerivedClassBody };
enum class PropertyType {
  Normal,
  Shorthand,
  CoverInitializedName,
  Getter,
  Setter,
  Method,
  GeneratorMethod,
  AsyncMethod,
  AsyncGeneratorMethod,
  Constructor,
  DerivedConstructor,
  Field,
};

enum AwaitHandling : uint8_t {
  AwaitIsName,
  AwaitIsKeyword,
  AwaitIsModuleKeyword
};

template <class ParseHandler, typename Unit>
class AutoAwaitIsKeyword;

template <class ParseHandler, typename Unit>
class AutoInParametersOfAsyncFunction;

class MOZ_STACK_CLASS ParserSharedBase : public JS::CustomAutoRooter {
 public:
  enum class Kind { Parser, BinASTParser };

  ParserSharedBase(JSContext* cx, CompilationInfo& compilationInfo, Kind kind);
  ~ParserSharedBase();

 public:
  JSContext* const cx_;

  LifoAlloc& alloc_;

  // Information for parsing with a lifetime longer than the parser itself.
  CompilationInfo& compilationInfo_;

  // innermost parse context (stack-allocated)
  ParseContext* pc_;

  // For tracking used names in this parsing session.
  UsedNameTracker& usedNames_;

 public:
  CompilationInfo& getCompilationInfo() { return compilationInfo_; }
};

class MOZ_STACK_CLASS ParserBase : public ParserSharedBase,
                                   public ErrorReportMixin {
  using Base = ErrorReportMixin;

 public:
  TokenStreamAnyChars anyChars;

  ScriptSource* ss;

  // Perform constant-folding; must be true when interfacing with the emitter.
  const bool foldConstants_ : 1;

 protected:
#if DEBUG
  /* Our fallible 'checkOptions' member function has been called. */
  bool checkOptionsCalled_ : 1;
#endif

  /* Unexpected end of input, i.e. Eof not at top-level. */
  bool isUnexpectedEOF_ : 1;

  /* AwaitHandling */ uint8_t awaitHandling_ : 2;

  bool inParametersOfAsyncFunction_ : 1;

 public:
  bool awaitIsKeyword() const { return awaitHandling_ != AwaitIsName; }

  bool inParametersOfAsyncFunction() const {
    return inParametersOfAsyncFunction_;
  }

  ParseGoal parseGoal() const {
    return pc_->sc()->hasModuleGoal() ? ParseGoal::Module : ParseGoal::Script;
  }

  template <class, typename>
  friend class AutoAwaitIsKeyword;
  template <class, typename>
  friend class AutoInParametersOfAsyncFunction;

  ParserBase(JSContext* cx, const JS::ReadOnlyCompileOptions& options,
             bool foldConstants, CompilationInfo& compilationInfo);
  ~ParserBase();

  bool checkOptions();

  void trace(JSTracer* trc) final {}

  const char* getFilename() const { return anyChars.getFilename(); }
  TokenPos pos() const { return anyChars.currentToken().pos; }

  // Determine whether |yield| is a valid name in the current context.
  bool yieldExpressionsSupported() const { return pc_->isGenerator(); }

  bool setLocalStrictMode(bool strict) {
    MOZ_ASSERT(anyChars.debugHasNoLookahead());
    return pc_->sc()->setLocalStrictMode(strict);
  }

 public:
  // Implement ErrorReportMixin.

  JSContext* getContext() const override { return cx_; }

  bool strictMode() const override { return pc_->sc()->strict(); }

  const JS::ReadOnlyCompileOptions& options() const override {
    return anyChars.options();
  }

  using Base::error;
  using Base::errorAt;
  using Base::errorNoOffset;
  using Base::errorWithNotes;
  using Base::errorWithNotesAt;
  using Base::errorWithNotesNoOffset;
  using Base::strictModeError;
  using Base::strictModeErrorAt;
  using Base::strictModeErrorNoOffset;
  using Base::strictModeErrorWithNotes;
  using Base::strictModeErrorWithNotesAt;
  using Base::strictModeErrorWithNotesNoOffset;
  using Base::warning;
  using Base::warningAt;
  using Base::warningNoOffset;
  using Base::warningWithNotes;
  using Base::warningWithNotesAt;
  using Base::warningWithNotesNoOffset;

 public:
  bool isUnexpectedEOF() const { return isUnexpectedEOF_; }

  bool isValidStrictBinding(PropertyName* name);

  bool hasValidSimpleStrictParameterNames();

  /*
   * Create a new function object given a name (which is optional if this is
   * a function expression).
   */
  JSFunction* newFunction(HandleAtom atom, FunctionSyntaxKind kind,
                          GeneratorKind generatorKind,
                          FunctionAsyncKind asyncKind);

  // A Parser::Mark is the extension of the LifoAlloc::Mark to the entire
  // Parser's state. Note: clients must still take care that any ParseContext
  // that points into released ParseNodes is destroyed.
  class Mark {
    friend class ParserBase;
    LifoAlloc::Mark mark;
    FunctionBox* traceListHead;
  };
  Mark mark() const {
    Mark m;
    m.mark = alloc_.mark();
    m.traceListHead = compilationInfo_.traceListHead;
    return m;
  }
  void release(Mark m) {
    alloc_.release(m.mark);
    compilationInfo_.traceListHead = m.traceListHead;
  }

 public:
  mozilla::Maybe<GlobalScope::Data*> newGlobalScopeData(
      ParseContext::Scope& scope);
  mozilla::Maybe<ModuleScope::Data*> newModuleScopeData(
      ParseContext::Scope& scope);
  mozilla::Maybe<EvalScope::Data*> newEvalScopeData(ParseContext::Scope& scope);
  mozilla::Maybe<FunctionScope::Data*> newFunctionScopeData(
      ParseContext::Scope& scope, bool hasParameterExprs,
      IsFieldInitializer isFieldInitializer);
  mozilla::Maybe<VarScope::Data*> newVarScopeData(ParseContext::Scope& scope);
  mozilla::Maybe<LexicalScope::Data*> newLexicalScopeData(
      ParseContext::Scope& scope);

 protected:
  enum InvokedPrediction { PredictUninvoked = false, PredictInvoked = true };
  enum ForInitLocation { InForInit, NotInForInit };

  // While on a |let| Name token, examine |next| (which must already be
  // gotten).  Indicate whether |next|, the next token already gotten with
  // modifier TokenStream::SlashIsDiv, continues a LexicalDeclaration.
  bool nextTokenContinuesLetDeclaration(TokenKind next);

  bool noteUsedNameInternal(HandlePropertyName name);

  bool checkAndMarkSuperScope();

  bool leaveInnerFunction(ParseContext* outerpc);

  JSAtom* prefixAccessorName(PropertyType propType, HandleAtom propAtom);

  MOZ_MUST_USE bool setSourceMapInfo();

  void setFunctionEndFromCurrentToken(FunctionBox* funbox) const;
};

template <class ParseHandler>
class MOZ_STACK_CLASS PerHandlerParser : public ParserBase {
  using Base = ParserBase;

 private:
  using Node = typename ParseHandler::Node;

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
  using longTypeName = typename ParseHandler::longTypeName;
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

 protected:
  /* State specific to the kind of parse being performed. */
  ParseHandler handler_;

  // When ParseHandler is FullParseHandler:
  //
  //   If non-null, this field holds the syntax parser used to attempt lazy
  //   parsing of inner functions. If null, then lazy parsing is disabled.
  //
  // When ParseHandler is SyntaxParseHandler:
  //
  //   If non-null, this field must be a sentinel value signaling that the
  //   syntax parse was aborted. If null, then lazy parsing was aborted due
  //   to encountering unsupported language constructs.
  //
  // |internalSyntaxParser_| is really a |Parser<SyntaxParseHandler, Unit>*|
  // where |Unit| varies per |Parser<ParseHandler, Unit>|.  But this
  // template class doesn't know |Unit|, so we store a |void*| here and make
  // |GeneralParser<ParseHandler, Unit>::getSyntaxParser| impose the real type.
  void* internalSyntaxParser_;

 private:
  // NOTE: The argument ordering here is deliberately different from the
  //       public constructor so that typos calling the public constructor
  //       are less likely to select this overload.
  PerHandlerParser(JSContext* cx, const JS::ReadOnlyCompileOptions& options,
                   bool foldConstants, CompilationInfo& compilationInfo,
                   BaseScript* lazyOuterFunction, void* internalSyntaxParser);

 protected:
  template <typename Unit>
  PerHandlerParser(JSContext* cx, const JS::ReadOnlyCompileOptions& options,
                   bool foldConstants, CompilationInfo& compilationInfo,
                   GeneralParser<SyntaxParseHandler, Unit>* syntaxParser,
                   BaseScript* lazyOuterFunction)
      : PerHandlerParser(cx, options, foldConstants, compilationInfo,
                         lazyOuterFunction, static_cast<void*>(syntaxParser)) {}

  static typename ParseHandler::NullNode null() { return ParseHandler::null(); }

  NameNodeType stringLiteral();

  const char* nameIsArgumentsOrEval(Node node);

  bool noteDestructuredPositionalFormalParameter(FunctionNodeType funNode,
                                                 Node destruct);

  bool noteUsedName(HandlePropertyName name) {
    // If the we are delazifying, the BaseScript already has all the closed-over
    // info for bindings and there's no need to track used names.
    if (handler_.canSkipLazyClosedOverBindings()) {
      return true;
    }

    return ParserBase::noteUsedNameInternal(name);
  }

  // Required on Scope exit.
  bool propagateFreeNamesAndMarkClosedOverBindings(ParseContext::Scope& scope);

  bool finishFunctionScopes(bool isStandaloneFunction);
  LexicalScopeNodeType finishLexicalScope(ParseContext::Scope& scope, Node body,
                                          ScopeKind kind = ScopeKind::Lexical);
  bool finishFunction(
      bool isStandaloneFunction = false,
      IsFieldInitializer isFieldInitializer = IsFieldInitializer::No);

  inline NameNodeType newName(PropertyName* name);
  inline NameNodeType newName(PropertyName* name, TokenPos pos);

  NameNodeType newInternalDotName(HandlePropertyName name);
  NameNodeType newThisName();
  NameNodeType newDotGeneratorName();

  NameNodeType identifierReference(Handle<PropertyName*> name);

  Node noSubstitutionTaggedTemplate();

  inline bool processExport(Node node);
  inline bool processExportFrom(BinaryNodeType node);

  // If ParseHandler is SyntaxParseHandler:
  //   Do nothing.
  // If ParseHandler is FullParseHandler:
  //   Disable syntax parsing of all future inner functions during this
  //   full-parse.
  inline void disableSyntaxParser();

  // If ParseHandler is SyntaxParseHandler:
  //   Flag the current syntax parse as aborted due to unsupported language
  //   constructs and return false.  Aborting the current syntax parse does
  //   not disable attempts to syntax-parse future inner functions.
  // If ParseHandler is FullParseHandler:
  //    Disable syntax parsing of all future inner functions and return true.
  inline bool abortIfSyntaxParser();

  // If ParseHandler is SyntaxParseHandler:
  //   Return whether the last syntax parse was aborted due to unsupported
  //   language constructs.
  // If ParseHandler is FullParseHandler:
  //   Return false.
  inline bool hadAbortedSyntaxParse();

  // If ParseHandler is SyntaxParseHandler:
  //   Clear whether the last syntax parse was aborted.
  // If ParseHandler is FullParseHandler:
  //   Do nothing.
  inline void clearAbortedSyntaxParse();

 public:
  NameNodeType newPropertyName(PropertyName* key, const TokenPos& pos) {
    return handler_.newPropertyName(key, pos);
  }

  PropertyAccessType newPropertyAccess(Node expr, NameNodeType key) {
    return handler_.newPropertyAccess(expr, key);
  }

  FunctionBox* newFunctionBox(FunctionNodeType funNode, JSFunction* fun,
                              uint32_t toStringStart, Directives directives,
                              GeneratorKind generatorKind,
                              FunctionAsyncKind asyncKind);

  FunctionBox* newFunctionBox(FunctionNodeType funNode, HandleAtom explicitName,
                              FunctionFlags flags, uint32_t toStringStart,
                              Directives directives,
                              GeneratorKind generatorKind,
                              FunctionAsyncKind asyncKind);

 public:
  // ErrorReportMixin.

  using Base::error;
  using Base::errorAt;
  using Base::errorNoOffset;
  using Base::errorWithNotes;
  using Base::errorWithNotesAt;
  using Base::errorWithNotesNoOffset;
  using Base::strictModeError;
  using Base::strictModeErrorAt;
  using Base::strictModeErrorNoOffset;
  using Base::strictModeErrorWithNotes;
  using Base::strictModeErrorWithNotesAt;
  using Base::strictModeErrorWithNotesNoOffset;
  using Base::warning;
  using Base::warningAt;
  using Base::warningNoOffset;
  using Base::warningWithNotes;
  using Base::warningWithNotesAt;
  using Base::warningWithNotesNoOffset;
};

#define ABORTED_SYNTAX_PARSE_SENTINEL reinterpret_cast<void*>(0x1)

template <>
inline void PerHandlerParser<SyntaxParseHandler>::disableSyntaxParser() {}

template <>
inline bool PerHandlerParser<SyntaxParseHandler>::abortIfSyntaxParser() {
  internalSyntaxParser_ = ABORTED_SYNTAX_PARSE_SENTINEL;
  return false;
}

template <>
inline bool PerHandlerParser<SyntaxParseHandler>::hadAbortedSyntaxParse() {
  return internalSyntaxParser_ == ABORTED_SYNTAX_PARSE_SENTINEL;
}

template <>
inline void PerHandlerParser<SyntaxParseHandler>::clearAbortedSyntaxParse() {
  internalSyntaxParser_ = nullptr;
}

#undef ABORTED_SYNTAX_PARSE_SENTINEL

// Disable syntax parsing of all future inner functions during this
// full-parse.
template <>
inline void PerHandlerParser<FullParseHandler>::disableSyntaxParser() {
  internalSyntaxParser_ = nullptr;
}

template <>
inline bool PerHandlerParser<FullParseHandler>::abortIfSyntaxParser() {
  disableSyntaxParser();
  return true;
}

template <>
inline bool PerHandlerParser<FullParseHandler>::hadAbortedSyntaxParse() {
  return false;
}

template <>
inline void PerHandlerParser<FullParseHandler>::clearAbortedSyntaxParse() {}

template <class Parser>
class ParserAnyCharsAccess {
 public:
  using TokenStreamSpecific = typename Parser::TokenStream;
  using GeneralTokenStreamChars =
      typename TokenStreamSpecific::GeneralCharsBase;

  static inline TokenStreamAnyChars& anyChars(GeneralTokenStreamChars* ts);
  static inline const TokenStreamAnyChars& anyChars(
      const GeneralTokenStreamChars* ts);
};

// Specify a value for an ES6 grammar parametrization.  We have no enum for
// [Return] because its behavior is exactly equivalent to checking whether
// we're in a function box -- easier and simpler than passing an extra
// parameter everywhere.
enum YieldHandling { YieldIsName, YieldIsKeyword };
enum InHandling { InAllowed, InProhibited };
enum DefaultHandling { NameRequired, AllowDefaultName };
enum TripledotHandling { TripledotAllowed, TripledotProhibited };

template <class ParseHandler, typename Unit>
class Parser;

template <class ParseHandler, typename Unit>
class MOZ_STACK_CLASS GeneralParser : public PerHandlerParser<ParseHandler> {
 public:
  using TokenStream =
      TokenStreamSpecific<Unit, ParserAnyCharsAccess<GeneralParser>>;

 private:
  using Base = PerHandlerParser<ParseHandler>;
  using FinalParser = Parser<ParseHandler, Unit>;
  using Node = typename ParseHandler::Node;

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
  using longTypeName = typename ParseHandler::longTypeName;
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

  using typename Base::InvokedPrediction;
  using SyntaxParser = Parser<SyntaxParseHandler, Unit>;

 protected:
  using Modifier = TokenStreamShared::Modifier;
  using Position = typename TokenStream::Position;

  using Base::PredictInvoked;
  using Base::PredictUninvoked;

  using Base::alloc_;
  using Base::awaitIsKeyword;
  using Base::inParametersOfAsyncFunction;
  using Base::parseGoal;
#if DEBUG
  using Base::checkOptionsCalled_;
#endif
  using Base::finishFunctionScopes;
  using Base::finishLexicalScope;
  using Base::foldConstants_;
  using Base::getFilename;
  using Base::hasValidSimpleStrictParameterNames;
  using Base::isUnexpectedEOF_;
  using Base::nameIsArgumentsOrEval;
  using Base::newFunction;
  using Base::newFunctionBox;
  using Base::newName;
  using Base::null;
  using Base::options;
  using Base::pos;
  using Base::propagateFreeNamesAndMarkClosedOverBindings;
  using Base::setLocalStrictMode;
  using Base::stringLiteral;
  using Base::yieldExpressionsSupported;

  using Base::abortIfSyntaxParser;
  using Base::clearAbortedSyntaxParse;
  using Base::disableSyntaxParser;
  using Base::hadAbortedSyntaxParse;

 public:
  // Implement ErrorReportMixin.

  MOZ_MUST_USE bool computeErrorMetadata(
      ErrorMetadata* err, const ErrorReportMixin::ErrorOffset& offset) override;

  using Base::error;
  using Base::errorAt;
  using Base::errorNoOffset;
  using Base::errorWithNotes;
  using Base::errorWithNotesAt;
  using Base::errorWithNotesNoOffset;
  using Base::strictModeError;
  using Base::strictModeErrorAt;
  using Base::strictModeErrorNoOffset;
  using Base::strictModeErrorWithNotes;
  using Base::strictModeErrorWithNotesAt;
  using Base::strictModeErrorWithNotesNoOffset;
  using Base::warning;
  using Base::warningAt;
  using Base::warningNoOffset;
  using Base::warningWithNotes;
  using Base::warningWithNotesAt;
  using Base::warningWithNotesNoOffset;

 public:
  using Base::anyChars;
  using Base::cx_;
  using Base::handler_;
  using Base::pc_;
  using Base::usedNames_;

 private:
  using Base::checkAndMarkSuperScope;
  using Base::finishFunction;
  using Base::identifierReference;
  using Base::leaveInnerFunction;
  using Base::newDotGeneratorName;
  using Base::newInternalDotName;
  using Base::newThisName;
  using Base::nextTokenContinuesLetDeclaration;
  using Base::noSubstitutionTaggedTemplate;
  using Base::noteDestructuredPositionalFormalParameter;
  using Base::noteUsedName;
  using Base::prefixAccessorName;
  using Base::processExport;
  using Base::processExportFrom;
  using Base::setFunctionEndFromCurrentToken;

 private:
  inline FinalParser* asFinalParser();
  inline const FinalParser* asFinalParser() const;

  /*
   * A class for temporarily stashing errors while parsing continues.
   *
   * The ability to stash an error is useful for handling situations where we
   * aren't able to verify that an error has occurred until later in the parse.
   * For instance | ({x=1}) | is always parsed as an object literal with
   * a SyntaxError, however, in the case where it is followed by '=>' we rewind
   * and reparse it as a valid arrow function. Here a PossibleError would be
   * set to 'pending' when the initial SyntaxError was encountered then
   * 'resolved' just before rewinding the parser.
   *
   * There are currently two kinds of PossibleErrors: Expression and
   * Destructuring errors. Expression errors are used to mark a possible
   * syntax error when a grammar production is used in an expression context.
   * For example in |{x = 1}|, we mark the CoverInitializedName |x = 1| as a
   * possible expression error, because CoverInitializedName productions
   * are disallowed when an actual ObjectLiteral is expected.
   * Destructuring errors are used to record possible syntax errors in
   * destructuring contexts. For example in |[...rest, ] = []|, we initially
   * mark the trailing comma after the spread expression as a possible
   * destructuring error, because the ArrayAssignmentPattern grammar
   * production doesn't allow a trailing comma after the rest element.
   *
   * When using PossibleError one should set a pending error at the location
   * where an error occurs. From that point, the error may be resolved
   * (invalidated) or left until the PossibleError is checked.
   *
   * Ex:
   *   PossibleError possibleError(*this);
   *   possibleError.setPendingExpressionErrorAt(pos, JSMSG_BAD_PROP_ID);
   *   // A JSMSG_BAD_PROP_ID ParseError is reported, returns false.
   *   if (!possibleError.checkForExpressionError()) {
   *       return false; // we reach this point with a pending exception
   *   }
   *
   *   PossibleError possibleError(*this);
   *   possibleError.setPendingExpressionErrorAt(pos, JSMSG_BAD_PROP_ID);
   *   // Returns true, no error is reported.
   *   if (!possibleError.checkForDestructuringError()) {
   *       return false; // not reached, no pending exception
   *   }
   *
   *   PossibleError possibleError(*this);
   *   // Returns true, no error is reported.
   *   if (!possibleError.checkForExpressionError()) {
   *       return false; // not reached, no pending exception
   *   }
   */
  class MOZ_STACK_CLASS PossibleError {
   private:
    enum class ErrorKind { Expression, Destructuring, DestructuringWarning };

    enum class ErrorState { None, Pending };

    struct Error {
      ErrorState state_ = ErrorState::None;

      // Error reporting fields.
      uint32_t offset_;
      unsigned errorNumber_;
    };

    GeneralParser<ParseHandler, Unit>& parser_;
    Error exprError_;
    Error destructuringError_;
    Error destructuringWarning_;

    // Returns the error report.
    Error& error(ErrorKind kind);

    // Return true if an error is pending without reporting.
    bool hasError(ErrorKind kind);

    // Resolve any pending error.
    void setResolved(ErrorKind kind);

    // Set a pending error. Only a single error may be set per instance and
    // error kind.
    void setPending(ErrorKind kind, const TokenPos& pos, unsigned errorNumber);

    // If there is a pending error, report it and return false, otherwise
    // return true.
    MOZ_MUST_USE bool checkForError(ErrorKind kind);

    // Transfer an existing error to another instance.
    void transferErrorTo(ErrorKind kind, PossibleError* other);

   public:
    explicit PossibleError(GeneralParser<ParseHandler, Unit>& parser);

    // Return true if a pending destructuring error is present.
    bool hasPendingDestructuringError();

    // Set a pending destructuring error. Only a single error may be set
    // per instance, i.e. subsequent calls to this method are ignored and
    // won't overwrite the existing pending error.
    void setPendingDestructuringErrorAt(const TokenPos& pos,
                                        unsigned errorNumber);

    // Set a pending destructuring warning. Only a single warning may be
    // set per instance, i.e. subsequent calls to this method are ignored
    // and won't overwrite the existing pending warning.
    void setPendingDestructuringWarningAt(const TokenPos& pos,
                                          unsigned errorNumber);

    // Set a pending expression error. Only a single error may be set per
    // instance, i.e. subsequent calls to this method are ignored and won't
    // overwrite the existing pending error.
    void setPendingExpressionErrorAt(const TokenPos& pos, unsigned errorNumber);

    // If there is a pending destructuring error or warning, report it and
    // return false, otherwise return true. Clears any pending expression
    // error.
    MOZ_MUST_USE bool checkForDestructuringErrorOrWarning();

    // If there is a pending expression error, report it and return false,
    // otherwise return true. Clears any pending destructuring error or
    // warning.
    MOZ_MUST_USE bool checkForExpressionError();

    // Pass pending errors between possible error instances. This is useful
    // for extending the lifetime of a pending error beyond the scope of
    // the PossibleError where it was initially set (keeping in mind that
    // PossibleError is a MOZ_STACK_CLASS).
    void transferErrorsTo(PossibleError* other);
  };

 protected:
  SyntaxParser* getSyntaxParser() const {
    return reinterpret_cast<SyntaxParser*>(Base::internalSyntaxParser_);
  }

 public:
  TokenStream tokenStream;

 public:
  GeneralParser(JSContext* cx, const JS::ReadOnlyCompileOptions& options,
                const Unit* units, size_t length, bool foldConstants,
                CompilationInfo& compilationInfo, SyntaxParser* syntaxParser,
                BaseScript* lazyOuterFunction);

  inline void setAwaitHandling(AwaitHandling awaitHandling);
  inline void setInParametersOfAsyncFunction(bool inParameters);

  /*
   * Parse a top-level JS script.
   */
  ListNodeType parse();

 private:
  /*
   * Gets the next token and checks if it matches to the given `condition`.
   * If it matches, returns true.
   * If it doesn't match, calls `errorReport` to report the error, and
   * returns false.
   * If other error happens, it returns false but `errorReport` may not be
   * called and other error will be thrown in that case.
   *
   * In any case, the already gotten token is not ungotten.
   *
   * The signature of `condition` is [...](TokenKind actual) -> bool, and
   * the signature of `errorReport` is [...](TokenKind actual).
   */
  template <typename ConditionT, typename ErrorReportT>
  MOZ_MUST_USE bool mustMatchTokenInternal(ConditionT condition,
                                           ErrorReportT errorReport);

 public:
  /*
   * The following mustMatchToken variants follow the behavior and parameter
   * types of mustMatchTokenInternal above.
   *
   * If modifier is omitted, `SlashIsDiv` is used.
   * If TokenKind is passed instead of `condition`, it checks if the next
   * token is the passed token.
   * If error number is passed instead of `errorReport`, it reports an
   * error with the passed errorNumber.
   */
  MOZ_MUST_USE bool mustMatchToken(TokenKind expected, JSErrNum errorNumber) {
    return mustMatchTokenInternal(
        [expected](TokenKind actual) { return actual == expected; },
        [this, errorNumber](TokenKind) { this->error(errorNumber); });
  }

  template <typename ConditionT>
  MOZ_MUST_USE bool mustMatchToken(ConditionT condition, JSErrNum errorNumber) {
    return mustMatchTokenInternal(condition, [this, errorNumber](TokenKind) {
      this->error(errorNumber);
    });
  }

  template <typename ErrorReportT>
  MOZ_MUST_USE bool mustMatchToken(TokenKind expected,
                                   ErrorReportT errorReport) {
    return mustMatchTokenInternal(
        [expected](TokenKind actual) { return actual == expected; },
        errorReport);
  }

 private:
  NameNodeType noSubstitutionUntaggedTemplate();
  ListNodeType templateLiteral(YieldHandling yieldHandling);
  bool taggedTemplate(YieldHandling yieldHandling, ListNodeType tagArgsList,
                      TokenKind tt);
  bool appendToCallSiteObj(CallSiteNodeType callSiteObj);
  bool addExprAndGetNextTemplStrToken(YieldHandling yieldHandling,
                                      ListNodeType nodeList, TokenKind* ttp);

  inline bool trySyntaxParseInnerFunction(
      FunctionNodeType* funNode, HandleAtom explicitName, FunctionFlags flags,
      uint32_t toStringStart, InHandling inHandling,
      YieldHandling yieldHandling, FunctionSyntaxKind kind,
      GeneratorKind generatorKind, FunctionAsyncKind asyncKind, bool tryAnnexB,
      Directives inheritedDirectives, Directives* newDirectives);

  inline bool skipLazyInnerFunction(FunctionNodeType funNode,
                                    uint32_t toStringStart,
                                    FunctionSyntaxKind kind, bool tryAnnexB);

  void setFunctionStartAtCurrentToken(FunctionBox* funbox) const;

 public:
  /* Public entry points for parsing. */
  Node statementListItem(YieldHandling yieldHandling,
                         bool canHaveDirectives = false);

  // Parse an inner function given an enclosing ParseContext and a
  // FunctionBox for the inner function.
  MOZ_MUST_USE FunctionNodeType innerFunctionForFunctionBox(
      FunctionNodeType funNode, ParseContext* outerpc, FunctionBox* funbox,
      InHandling inHandling, YieldHandling yieldHandling,
      FunctionSyntaxKind kind, Directives* newDirectives);

  // Parse a function's formal parameters and its body assuming its function
  // ParseContext is already on the stack.
  bool functionFormalParametersAndBody(
      InHandling inHandling, YieldHandling yieldHandling,
      FunctionNodeType* funNode, FunctionSyntaxKind kind,
      const mozilla::Maybe<uint32_t>& parameterListEnd = mozilla::Nothing(),
      bool isStandaloneFunction = false);

 private:
  /*
   * JS parsers, from lowest to highest precedence.
   *
   * Each parser must be called during the dynamic scope of a ParseContext
   * object, pointed to by this->pc_.
   *
   * Each returns a parse node tree or null on error.
   */
  FunctionNodeType functionStmt(
      uint32_t toStringStart, YieldHandling yieldHandling,
      DefaultHandling defaultHandling,
      FunctionAsyncKind asyncKind = FunctionAsyncKind::SyncFunction);
  FunctionNodeType functionExpr(uint32_t toStringStart,
                                InvokedPrediction invoked,
                                FunctionAsyncKind asyncKind);

  Node statement(YieldHandling yieldHandling);
  bool maybeParseDirective(ListNodeType list, Node pn, bool* cont);

  LexicalScopeNodeType blockStatement(
      YieldHandling yieldHandling,
      unsigned errorNumber = JSMSG_CURLY_IN_COMPOUND);
  BinaryNodeType doWhileStatement(YieldHandling yieldHandling);
  BinaryNodeType whileStatement(YieldHandling yieldHandling);

  Node forStatement(YieldHandling yieldHandling);
  bool forHeadStart(YieldHandling yieldHandling, ParseNodeKind* forHeadKind,
                    Node* forInitialPart,
                    mozilla::Maybe<ParseContext::Scope>& forLetImpliedScope,
                    Node* forInOrOfExpression);
  Node expressionAfterForInOrOf(ParseNodeKind forHeadKind,
                                YieldHandling yieldHandling);

  SwitchStatementType switchStatement(YieldHandling yieldHandling);
  ContinueStatementType continueStatement(YieldHandling yieldHandling);
  BreakStatementType breakStatement(YieldHandling yieldHandling);
  UnaryNodeType returnStatement(YieldHandling yieldHandling);
  BinaryNodeType withStatement(YieldHandling yieldHandling);
  UnaryNodeType throwStatement(YieldHandling yieldHandling);
  TernaryNodeType tryStatement(YieldHandling yieldHandling);
  LexicalScopeNodeType catchBlockStatement(
      YieldHandling yieldHandling, ParseContext::Scope& catchParamScope);
  DebuggerStatementType debuggerStatement();

  ListNodeType variableStatement(YieldHandling yieldHandling);

  LabeledStatementType labeledStatement(YieldHandling yieldHandling);
  Node labeledItem(YieldHandling yieldHandling);

  TernaryNodeType ifStatement(YieldHandling yieldHandling);
  Node consequentOrAlternative(YieldHandling yieldHandling);

  ListNodeType lexicalDeclaration(YieldHandling yieldHandling,
                                  DeclarationKind kind);

  inline BinaryNodeType importDeclaration();
  Node importDeclarationOrImportExpr(YieldHandling yieldHandling);

  BinaryNodeType exportFrom(uint32_t begin, Node specList);
  BinaryNodeType exportBatch(uint32_t begin);
  inline bool checkLocalExportNames(ListNodeType node);
  Node exportClause(uint32_t begin);
  UnaryNodeType exportFunctionDeclaration(
      uint32_t begin, uint32_t toStringStart,
      FunctionAsyncKind asyncKind = FunctionAsyncKind::SyncFunction);
  UnaryNodeType exportVariableStatement(uint32_t begin);
  UnaryNodeType exportClassDeclaration(uint32_t begin);
  UnaryNodeType exportLexicalDeclaration(uint32_t begin, DeclarationKind kind);
  BinaryNodeType exportDefaultFunctionDeclaration(
      uint32_t begin, uint32_t toStringStart,
      FunctionAsyncKind asyncKind = FunctionAsyncKind::SyncFunction);
  BinaryNodeType exportDefaultClassDeclaration(uint32_t begin);
  BinaryNodeType exportDefaultAssignExpr(uint32_t begin);
  BinaryNodeType exportDefault(uint32_t begin);
  Node exportDeclaration();

  UnaryNodeType expressionStatement(
      YieldHandling yieldHandling,
      InvokedPrediction invoked = PredictUninvoked);

  // Declaration parsing.  The main entrypoint is Parser::declarationList,
  // with sub-functionality split out into the remaining methods.

  // |blockScope| may be non-null only when |kind| corresponds to a lexical
  // declaration (that is, PNK_LET or PNK_CONST).
  //
  // The for* parameters, for normal declarations, should be null/ignored.
  // They should be non-null only when Parser::forHeadStart parses a
  // declaration at the start of a for-loop head.
  //
  // In this case, on success |*forHeadKind| is PNK_FORHEAD, PNK_FORIN, or
  // PNK_FOROF, corresponding to the three for-loop kinds.  The precise value
  // indicates what was parsed.
  //
  // If parsing recognized a for(;;) loop, the next token is the ';' within
  // the loop-head that separates the init/test parts.
  //
  // Otherwise, for for-in/of loops, the next token is the ')' ending the
  // loop-head.  Additionally, the expression that the loop iterates over was
  // parsed into |*forInOrOfExpression|.
  ListNodeType declarationList(YieldHandling yieldHandling, ParseNodeKind kind,
                               ParseNodeKind* forHeadKind = nullptr,
                               Node* forInOrOfExpression = nullptr);

  // The items in a declaration list are either patterns or names, with or
  // without initializers.  These two methods parse a single pattern/name and
  // any associated initializer -- and if parsing an |initialDeclaration|
  // will, if parsing in a for-loop head (as specified by |forHeadKind| being
  // non-null), consume additional tokens up to the closing ')' in a
  // for-in/of loop head, returning the iterated expression in
  // |*forInOrOfExpression|.  (An "initial declaration" is the first
  // declaration in a declaration list: |a| but not |b| in |var a, b|, |{c}|
  // but not |d| in |let {c} = 3, d|.)
  Node declarationPattern(DeclarationKind declKind, TokenKind tt,
                          bool initialDeclaration, YieldHandling yieldHandling,
                          ParseNodeKind* forHeadKind,
                          Node* forInOrOfExpression);
  Node declarationName(DeclarationKind declKind, TokenKind tt,
                       bool initialDeclaration, YieldHandling yieldHandling,
                       ParseNodeKind* forHeadKind, Node* forInOrOfExpression);

  // Having parsed a name (not found in a destructuring pattern) declared by
  // a declaration, with the current token being the '=' separating the name
  // from its initializer, parse and bind that initializer -- and possibly
  // consume trailing in/of and subsequent expression, if so directed by
  // |forHeadKind|.
  AssignmentNodeType initializerInNameDeclaration(NameNodeType binding,
                                                  DeclarationKind declKind,
                                                  bool initialDeclaration,
                                                  YieldHandling yieldHandling,
                                                  ParseNodeKind* forHeadKind,
                                                  Node* forInOrOfExpression);

  Node expr(InHandling inHandling, YieldHandling yieldHandling,
            TripledotHandling tripledotHandling,
            PossibleError* possibleError = nullptr,
            InvokedPrediction invoked = PredictUninvoked);
  Node assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                  TripledotHandling tripledotHandling,
                  PossibleError* possibleError = nullptr,
                  InvokedPrediction invoked = PredictUninvoked);
  Node assignExprWithoutYieldOrAwait(YieldHandling yieldHandling);
  UnaryNodeType yieldExpression(InHandling inHandling);
  Node condExpr(InHandling inHandling, YieldHandling yieldHandling,
                TripledotHandling tripledotHandling,
                PossibleError* possibleError, InvokedPrediction invoked);
  Node orExpr(InHandling inHandling, YieldHandling yieldHandling,
              TripledotHandling tripledotHandling, PossibleError* possibleError,
              InvokedPrediction invoked);
  Node unaryExpr(YieldHandling yieldHandling,
                 TripledotHandling tripledotHandling,
                 PossibleError* possibleError = nullptr,
                 InvokedPrediction invoked = PredictUninvoked);
  Node optionalExpr(YieldHandling yieldHandling,
                    TripledotHandling tripledotHandling, TokenKind tt,
                    PossibleError* possibleError = nullptr,
                    InvokedPrediction invoked = PredictUninvoked);
  Node memberExpr(YieldHandling yieldHandling,
                  TripledotHandling tripledotHandling, TokenKind tt,
                  bool allowCallSyntax, PossibleError* possibleError,
                  InvokedPrediction invoked);
  Node primaryExpr(YieldHandling yieldHandling,
                   TripledotHandling tripledotHandling, TokenKind tt,
                   PossibleError* possibleError, InvokedPrediction invoked);
  Node exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                    TripledotHandling tripledotHandling,
                    PossibleError* possibleError = nullptr);

  bool tryNewTarget(BinaryNodeType* newTarget);

  BinaryNodeType importExpr(YieldHandling yieldHandling, bool allowCallSyntax);

  FunctionNodeType methodDefinition(uint32_t toStringStart,
                                    PropertyType propType, HandleAtom funName);

  /*
   * Additional JS parsers.
   */
  bool functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                         FunctionNodeType funNode);

  FunctionNodeType functionDefinition(
      FunctionNodeType funNode, uint32_t toStringStart, InHandling inHandling,
      YieldHandling yieldHandling, HandleAtom name, FunctionSyntaxKind kind,
      GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
      bool tryAnnexB = false);

  // Parse a function body.  Pass StatementListBody if the body is a list of
  // statements; pass ExpressionBody if the body is a single expression.
  enum FunctionBodyType { StatementListBody, ExpressionBody };
  LexicalScopeNodeType functionBody(InHandling inHandling,
                                    YieldHandling yieldHandling,
                                    FunctionSyntaxKind kind,
                                    FunctionBodyType type);

  UnaryNodeType unaryOpExpr(YieldHandling yieldHandling, ParseNodeKind kind,
                            uint32_t begin);

  Node condition(InHandling inHandling, YieldHandling yieldHandling);

  ListNodeType argumentList(YieldHandling yieldHandling, bool* isSpread,
                            PossibleError* possibleError = nullptr);
  Node destructuringDeclaration(DeclarationKind kind,
                                YieldHandling yieldHandling, TokenKind tt);
  Node destructuringDeclarationWithoutYieldOrAwait(DeclarationKind kind,
                                                   YieldHandling yieldHandling,
                                                   TokenKind tt);

  inline bool checkExportedName(JSAtom* exportName);
  inline bool checkExportedNamesForArrayBinding(ListNodeType array);
  inline bool checkExportedNamesForObjectBinding(ListNodeType obj);
  inline bool checkExportedNamesForDeclaration(Node node);
  inline bool checkExportedNamesForDeclarationList(ListNodeType node);
  inline bool checkExportedNameForFunction(FunctionNodeType funNode);
  inline bool checkExportedNameForClass(ClassNodeType classNode);
  inline bool checkExportedNameForClause(NameNodeType nameNode);

  enum ClassContext { ClassStatement, ClassExpression };
  ClassNodeType classDefinition(YieldHandling yieldHandling,
                                ClassContext classContext,
                                DefaultHandling defaultHandling);
  struct ClassFields {
    // The number of instance class fields.
    size_t instanceFields = 0;

    // The number of instance class fields with computed property names.
    size_t instanceFieldKeys = 0;

    // The number of static class fields.
    size_t staticFields = 0;

    // The number of static class fields with computed property names.
    size_t staticFieldKeys = 0;
  };
  MOZ_MUST_USE bool classMember(YieldHandling yieldHandling,
                                const ParseContext::ClassStatement& classStmt,
                                HandlePropertyName className,
                                uint32_t classStartOffset,
                                HasHeritage hasHeritage,
                                ClassFields& classFields,
                                ListNodeType& classMembers, bool* done);
  MOZ_MUST_USE bool finishClassConstructor(
      const ParseContext::ClassStatement& classStmt,
      HandlePropertyName className, HasHeritage hasHeritage,
      uint32_t classStartOffset, uint32_t classEndOffset,
      const ClassFields& classFields, ListNodeType& classMembers);

  FunctionNodeType fieldInitializerOpt(Node name, HandleAtom atom,
                                       ClassFields& classFields, bool isStatic);
  FunctionNodeType synthesizeConstructor(HandleAtom className,
                                         uint32_t classNameOffset,
                                         HasHeritage hasHeritage);

  bool checkBindingIdentifier(PropertyName* ident, uint32_t offset,
                              YieldHandling yieldHandling,
                              TokenKind hint = TokenKind::Limit);

  PropertyName* labelOrIdentifierReference(YieldHandling yieldHandling);

  PropertyName* labelIdentifier(YieldHandling yieldHandling) {
    return labelOrIdentifierReference(yieldHandling);
  }

  PropertyName* identifierReference(YieldHandling yieldHandling) {
    return labelOrIdentifierReference(yieldHandling);
  }

  bool matchLabel(YieldHandling yieldHandling,
                  MutableHandle<PropertyName*> label);

  // Indicate if the next token (tokenized with SlashIsRegExp) is |in| or |of|.
  // If so, consume it.
  bool matchInOrOf(bool* isForInp, bool* isForOfp);

 private:
  bool checkIncDecOperand(Node operand, uint32_t operandOffset);
  bool checkStrictAssignment(Node lhs);

  void reportMissingClosing(unsigned errorNumber, unsigned noteNumber,
                            uint32_t openedPos);

  void reportRedeclaration(HandlePropertyName name, DeclarationKind prevKind,
                           TokenPos pos, uint32_t prevPos);
  bool notePositionalFormalParameter(FunctionNodeType funNode,
                                     HandlePropertyName name, uint32_t beginPos,
                                     bool disallowDuplicateParams,
                                     bool* duplicatedParam);

  enum PropertyNameContext {
    PropertyNameInLiteral,
    PropertyNameInPattern,
    PropertyNameInClass
  };
  Node propertyName(YieldHandling yieldHandling,
                    PropertyNameContext propertyNameContext,
                    const mozilla::Maybe<DeclarationKind>& maybeDecl,
                    ListNodeType propList, MutableHandleAtom propAtom);
  Node propertyOrMethodName(YieldHandling yieldHandling,
                            PropertyNameContext propertyNameContext,
                            const mozilla::Maybe<DeclarationKind>& maybeDecl,
                            ListNodeType propList, PropertyType* propType,
                            MutableHandleAtom propAtom);
  UnaryNodeType computedPropertyName(
      YieldHandling yieldHandling,
      const mozilla::Maybe<DeclarationKind>& maybeDecl,
      PropertyNameContext propertyNameContext, ListNodeType literal);
  ListNodeType arrayInitializer(YieldHandling yieldHandling,
                                PossibleError* possibleError);
  inline RegExpLiteralType newRegExp();

  ListNodeType objectLiteral(YieldHandling yieldHandling,
                             PossibleError* possibleError);

  BinaryNodeType bindingInitializer(Node lhs, DeclarationKind kind,
                                    YieldHandling yieldHandling);
  NameNodeType bindingIdentifier(DeclarationKind kind,
                                 YieldHandling yieldHandling);
  Node bindingIdentifierOrPattern(DeclarationKind kind,
                                  YieldHandling yieldHandling, TokenKind tt);
  ListNodeType objectBindingPattern(DeclarationKind kind,
                                    YieldHandling yieldHandling);
  ListNodeType arrayBindingPattern(DeclarationKind kind,
                                   YieldHandling yieldHandling);

  enum class TargetBehavior {
    PermitAssignmentPattern,
    ForbidAssignmentPattern
  };
  bool checkDestructuringAssignmentTarget(
      Node expr, TokenPos exprPos, PossibleError* exprPossibleError,
      PossibleError* possibleError,
      TargetBehavior behavior = TargetBehavior::PermitAssignmentPattern);
  void checkDestructuringAssignmentName(NameNodeType name, TokenPos namePos,
                                        PossibleError* possibleError);
  bool checkDestructuringAssignmentElement(Node expr, TokenPos exprPos,
                                           PossibleError* exprPossibleError,
                                           PossibleError* possibleError);

  NumericLiteralType newNumber(const Token& tok) {
    return handler_.newNumber(tok.number(), tok.decimalPoint(), tok.pos);
  }

  inline BigIntLiteralType newBigInt();

  JSAtom* bigIntAtom();

  enum class OptionalKind {
    NonOptional = 0,
    Optional,
  };
  Node memberPropertyAccess(
      Node lhs, OptionalKind optionalKind = OptionalKind::NonOptional);
  Node memberElemAccess(Node lhs, YieldHandling yieldHandling,
                        OptionalKind optionalKind = OptionalKind::NonOptional);
  Node memberSuperCall(Node lhs, YieldHandling yieldHandling);
  Node memberCall(TokenKind tt, Node lhs, YieldHandling yieldHandling,
                  PossibleError* possibleError,
                  OptionalKind optionalKind = OptionalKind::NonOptional);

 protected:
  // Match the current token against the BindingIdentifier production with
  // the given Yield parameter.  If there is no match, report a syntax
  // error.
  PropertyName* bindingIdentifier(YieldHandling yieldHandling);

  bool checkLabelOrIdentifierReference(PropertyName* ident, uint32_t offset,
                                       YieldHandling yieldHandling,
                                       TokenKind hint = TokenKind::Limit);

  ListNodeType statementList(YieldHandling yieldHandling);

  MOZ_MUST_USE FunctionNodeType innerFunction(
      FunctionNodeType funNode, ParseContext* outerpc, HandleAtom explicitName,
      FunctionFlags flags, uint32_t toStringStart, InHandling inHandling,
      YieldHandling yieldHandling, FunctionSyntaxKind kind,
      GeneratorKind generatorKind, FunctionAsyncKind asyncKind, bool tryAnnexB,
      Directives inheritedDirectives, Directives* newDirectives);

  // Implements Automatic Semicolon Insertion.
  //
  // Use this to match `;` in contexts where ASI is allowed. Call this after
  // ruling out all other possibilities except `;`, by peeking ahead if
  // necessary.
  //
  // Unlike most optional Modifiers, this method's `modifier` argument defaults
  // to SlashIsRegExp, since that's by far the most common case: usually an
  // optional semicolon is at the end of a statement or declaration, and the
  // next token could be a RegExp literal beginning a new ExpressionStatement.
  bool matchOrInsertSemicolon(Modifier modifier = TokenStream::SlashIsRegExp);

  bool noteDeclaredName(HandlePropertyName name, DeclarationKind kind,
                        TokenPos pos);

 private:
  inline bool asmJS(ListNodeType list);
};

template <typename Unit>
class MOZ_STACK_CLASS Parser<SyntaxParseHandler, Unit> final
    : public GeneralParser<SyntaxParseHandler, Unit> {
  using Base = GeneralParser<SyntaxParseHandler, Unit>;
  using Node = SyntaxParseHandler::Node;

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
  using longTypeName = SyntaxParseHandler::longTypeName;
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

  using SyntaxParser = Parser<SyntaxParseHandler, Unit>;

  // Numerous Base::* functions have bodies like
  //
  //   return asFinalParser()->func(...);
  //
  // and must be able to call functions here.  Add a friendship relationship
  // so functions here can be hidden when appropriate.
  friend class GeneralParser<SyntaxParseHandler, Unit>;

 public:
  using Base::Base;

  // Inherited types, listed here to have non-dependent names.
  using typename Base::Modifier;
  using typename Base::Position;
  using typename Base::TokenStream;

  // Inherited functions, listed here to have non-dependent names.

 public:
  using Base::anyChars;
  using Base::clearAbortedSyntaxParse;
  using Base::cx_;
  using Base::hadAbortedSyntaxParse;
  using Base::innerFunctionForFunctionBox;
  using Base::tokenStream;

 public:
  // ErrorReportMixin.

  using Base::error;
  using Base::errorAt;
  using Base::errorNoOffset;
  using Base::errorWithNotes;
  using Base::errorWithNotesAt;
  using Base::errorWithNotesNoOffset;
  using Base::strictModeError;
  using Base::strictModeErrorAt;
  using Base::strictModeErrorNoOffset;
  using Base::strictModeErrorWithNotes;
  using Base::strictModeErrorWithNotesAt;
  using Base::strictModeErrorWithNotesNoOffset;
  using Base::warning;
  using Base::warningAt;
  using Base::warningNoOffset;
  using Base::warningWithNotes;
  using Base::warningWithNotesAt;
  using Base::warningWithNotesNoOffset;

 private:
  using Base::alloc_;
#if DEBUG
  using Base::checkOptionsCalled_;
#endif
  using Base::finishFunctionScopes;
  using Base::functionFormalParametersAndBody;
  using Base::handler_;
  using Base::innerFunction;
  using Base::matchOrInsertSemicolon;
  using Base::mustMatchToken;
  using Base::newFunctionBox;
  using Base::newLexicalScopeData;
  using Base::newModuleScopeData;
  using Base::newName;
  using Base::noteDeclaredName;
  using Base::null;
  using Base::options;
  using Base::pc_;
  using Base::pos;
  using Base::propagateFreeNamesAndMarkClosedOverBindings;
  using Base::ss;
  using Base::statementList;
  using Base::stringLiteral;
  using Base::usedNames_;

 private:
  using Base::abortIfSyntaxParser;
  using Base::disableSyntaxParser;

 public:
  // Functions with multiple overloads of different visibility.  We can't
  // |using| the whole thing into existence because of the visibility
  // distinction, so we instead must manually delegate the required overload.

  PropertyName* bindingIdentifier(YieldHandling yieldHandling) {
    return Base::bindingIdentifier(yieldHandling);
  }

  // Functions present in both Parser<ParseHandler, Unit> specializations.

  inline void setAwaitHandling(AwaitHandling awaitHandling);
  inline void setInParametersOfAsyncFunction(bool inParameters);

  RegExpLiteralType newRegExp();
  BigIntLiteralType newBigInt();

  // Parse a module.
  ModuleNodeType moduleBody(ModuleSharedContext* modulesc);

  inline BinaryNodeType importDeclaration();
  inline bool checkLocalExportNames(ListNodeType node);
  inline bool checkExportedName(JSAtom* exportName);
  inline bool checkExportedNamesForArrayBinding(ListNodeType array);
  inline bool checkExportedNamesForObjectBinding(ListNodeType obj);
  inline bool checkExportedNamesForDeclaration(Node node);
  inline bool checkExportedNamesForDeclarationList(ListNodeType node);
  inline bool checkExportedNameForFunction(FunctionNodeType funNode);
  inline bool checkExportedNameForClass(ClassNodeType classNode);
  inline bool checkExportedNameForClause(NameNodeType nameNode);

  bool trySyntaxParseInnerFunction(
      FunctionNodeType* funNode, HandleAtom explicitName, FunctionFlags flags,
      uint32_t toStringStart, InHandling inHandling,
      YieldHandling yieldHandling, FunctionSyntaxKind kind,
      GeneratorKind generatorKind, FunctionAsyncKind asyncKind, bool tryAnnexB,
      Directives inheritedDirectives, Directives* newDirectives);

  bool skipLazyInnerFunction(FunctionNodeType funNode, uint32_t toStringStart,
                             FunctionSyntaxKind kind, bool tryAnnexB);

  bool asmJS(ListNodeType list);

  // Functions present only in Parser<SyntaxParseHandler, Unit>.
};

template <typename Unit>
class MOZ_STACK_CLASS Parser<FullParseHandler, Unit> final
    : public GeneralParser<FullParseHandler, Unit> {
  using Base = GeneralParser<FullParseHandler, Unit>;
  using Node = FullParseHandler::Node;

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
  using longTypeName = FullParseHandler::longTypeName;
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

  using SyntaxParser = Parser<SyntaxParseHandler, Unit>;

  // Numerous Base::* functions have bodies like
  //
  //   return asFinalParser()->func(...);
  //
  // and must be able to call functions here.  Add a friendship relationship
  // so functions here can be hidden when appropriate.
  friend class GeneralParser<FullParseHandler, Unit>;

 public:
  using Base::Base;

  // Inherited types, listed here to have non-dependent names.
  using typename Base::Modifier;
  using typename Base::Position;
  using typename Base::TokenStream;

  // Inherited functions, listed here to have non-dependent names.

 public:
  using Base::anyChars;
  using Base::clearAbortedSyntaxParse;
  using Base::functionFormalParametersAndBody;
  using Base::hadAbortedSyntaxParse;
  using Base::handler_;
  using Base::newFunctionBox;
  using Base::options;
  using Base::pc_;
  using Base::pos;
  using Base::ss;
  using Base::tokenStream;

 public:
  // ErrorReportMixin.

  using Base::error;
  using Base::errorAt;
  using Base::errorNoOffset;
  using Base::errorWithNotes;
  using Base::errorWithNotesAt;
  using Base::errorWithNotesNoOffset;
  using Base::strictModeError;
  using Base::strictModeErrorAt;
  using Base::strictModeErrorNoOffset;
  using Base::strictModeErrorWithNotes;
  using Base::strictModeErrorWithNotesAt;
  using Base::strictModeErrorWithNotesNoOffset;
  using Base::warning;
  using Base::warningAt;
  using Base::warningNoOffset;
  using Base::warningWithNotes;
  using Base::warningWithNotesAt;
  using Base::warningWithNotesNoOffset;

 private:
  using Base::alloc_;
  using Base::checkLabelOrIdentifierReference;
#if DEBUG
  using Base::checkOptionsCalled_;
#endif
  using Base::cx_;
  using Base::finishFunctionScopes;
  using Base::finishLexicalScope;
  using Base::innerFunction;
  using Base::innerFunctionForFunctionBox;
  using Base::matchOrInsertSemicolon;
  using Base::mustMatchToken;
  using Base::newEvalScopeData;
  using Base::newFunctionScopeData;
  using Base::newGlobalScopeData;
  using Base::newLexicalScopeData;
  using Base::newModuleScopeData;
  using Base::newName;
  using Base::newVarScopeData;
  using Base::noteDeclaredName;
  using Base::null;
  using Base::propagateFreeNamesAndMarkClosedOverBindings;
  using Base::statementList;
  using Base::stringLiteral;
  using Base::usedNames_;

  using Base::abortIfSyntaxParser;
  using Base::disableSyntaxParser;
  using Base::getSyntaxParser;

 public:
  // Functions with multiple overloads of different visibility.  We can't
  // |using| the whole thing into existence because of the visibility
  // distinction, so we instead must manually delegate the required overload.

  PropertyName* bindingIdentifier(YieldHandling yieldHandling) {
    return Base::bindingIdentifier(yieldHandling);
  }

  // Functions present in both Parser<ParseHandler, Unit> specializations.

  friend class AutoAwaitIsKeyword<SyntaxParseHandler, Unit>;
  inline void setAwaitHandling(AwaitHandling awaitHandling);

  friend class AutoInParametersOfAsyncFunction<SyntaxParseHandler, Unit>;
  inline void setInParametersOfAsyncFunction(bool inParameters);

  RegExpLiteralType newRegExp();
  BigIntLiteralType newBigInt();

  // Parse a module.
  ModuleNodeType moduleBody(ModuleSharedContext* modulesc);

  BinaryNodeType importDeclaration();
  bool checkLocalExportNames(ListNodeType node);
  bool checkExportedName(JSAtom* exportName);
  bool checkExportedNamesForArrayBinding(ListNodeType array);
  bool checkExportedNamesForObjectBinding(ListNodeType obj);
  bool checkExportedNamesForDeclaration(Node node);
  bool checkExportedNamesForDeclarationList(ListNodeType node);
  bool checkExportedNameForFunction(FunctionNodeType funNode);
  bool checkExportedNameForClass(ClassNodeType classNode);
  inline bool checkExportedNameForClause(NameNodeType nameNode);

  bool trySyntaxParseInnerFunction(
      FunctionNodeType* funNode, HandleAtom explicitName, FunctionFlags flags,
      uint32_t toStringStart, InHandling inHandling,
      YieldHandling yieldHandling, FunctionSyntaxKind kind,
      GeneratorKind generatorKind, FunctionAsyncKind asyncKind, bool tryAnnexB,
      Directives inheritedDirectives, Directives* newDirectives);

  MOZ_MUST_USE bool advancePastSyntaxParsedFunction(AutoKeepAtoms& keepAtoms,
                                                    SyntaxParser* syntaxParser);

  bool skipLazyInnerFunction(FunctionNodeType funNode, uint32_t toStringStart,
                             FunctionSyntaxKind kind, bool tryAnnexB);

  // Functions present only in Parser<FullParseHandler, Unit>.

  // Parse the body of an eval.
  //
  // Eval scripts are distinguished from global scripts in that in ES6, per
  // 18.2.1.1 steps 9 and 10, all eval scripts are executed under a fresh
  // lexical scope.
  LexicalScopeNodeType evalBody(EvalSharedContext* evalsc);

  // Parse a function, given only its arguments and body. Used for lazily
  // parsed functions.
  FunctionNodeType standaloneLazyFunction(HandleFunction fun,
                                          uint32_t toStringStart, bool strict,
                                          GeneratorKind generatorKind,
                                          FunctionAsyncKind asyncKind);

  // Parse a function, used for the Function, GeneratorFunction, and
  // AsyncFunction constructors.
  FunctionNodeType standaloneFunction(
      HandleFunction fun, HandleScope enclosingScope,
      const mozilla::Maybe<uint32_t>& parameterListEnd,
      GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
      Directives inheritedDirectives, Directives* newDirectives);

  bool checkStatementsEOF();

  // Parse the body of a global script.
  ListNodeType globalBody(GlobalSharedContext* globalsc);

  bool namedImportsOrNamespaceImport(TokenKind tt, ListNodeType importSpecSet);

  PropertyName* importedBinding() { return bindingIdentifier(YieldIsName); }

  bool checkLocalExportName(PropertyName* ident, uint32_t offset) {
    return checkLabelOrIdentifierReference(ident, offset, YieldIsName);
  }

  bool asmJS(ListNodeType list);
};

template <class Parser>
/* static */ inline const TokenStreamAnyChars&
ParserAnyCharsAccess<Parser>::anyChars(const GeneralTokenStreamChars* ts) {
  // The structure we're walking through looks like this:
  //
  //   struct ParserBase
  //   {
  //       ...;
  //       TokenStreamAnyChars anyChars;
  //       ...;
  //   };
  //   struct Parser : <class that ultimately inherits from ParserBase>
  //   {
  //       ...;
  //       TokenStreamSpecific tokenStream;
  //       ...;
  //   };
  //
  // We're passed a GeneralTokenStreamChars* (this being a base class of
  // Parser::tokenStream).  We cast that pointer to a TokenStreamSpecific*,
  // then translate that to the enclosing Parser*, then return the |anyChars|
  // member within.

  static_assert(std::is_base_of_v<GeneralTokenStreamChars, TokenStreamSpecific>,
                "the static_cast<> below assumes a base-class relationship");
  const auto* tss = static_cast<const TokenStreamSpecific*>(ts);

  auto tssAddr = reinterpret_cast<uintptr_t>(tss);

  using ActualTokenStreamType = decltype(std::declval<Parser>().tokenStream);
  static_assert(std::is_same_v<ActualTokenStreamType, TokenStreamSpecific>,
                "Parser::tokenStream must have type TokenStreamSpecific");

  uintptr_t parserAddr = tssAddr - offsetof(Parser, tokenStream);

  return reinterpret_cast<const Parser*>(parserAddr)->anyChars;
}

template <class Parser>
/* static */ inline TokenStreamAnyChars& ParserAnyCharsAccess<Parser>::anyChars(
    GeneralTokenStreamChars* ts) {
  const TokenStreamAnyChars& anyCharsConst =
      anyChars(const_cast<const GeneralTokenStreamChars*>(ts));

  return const_cast<TokenStreamAnyChars&>(anyCharsConst);
}

template <class ParseHandler, typename Unit>
class MOZ_STACK_CLASS AutoAwaitIsKeyword {
  using GeneralParser = frontend::GeneralParser<ParseHandler, Unit>;

 private:
  GeneralParser* parser_;
  AwaitHandling oldAwaitHandling_;

 public:
  AutoAwaitIsKeyword(GeneralParser* parser, AwaitHandling awaitHandling) {
    parser_ = parser;
    oldAwaitHandling_ = static_cast<AwaitHandling>(parser_->awaitHandling_);

    // 'await' is always a keyword in module contexts, so we don't modify
    // the state when the original handling is AwaitIsModuleKeyword.
    if (oldAwaitHandling_ != AwaitIsModuleKeyword) {
      parser_->setAwaitHandling(awaitHandling);
    }
  }

  ~AutoAwaitIsKeyword() { parser_->setAwaitHandling(oldAwaitHandling_); }
};

template <class ParseHandler, typename Unit>
class MOZ_STACK_CLASS AutoInParametersOfAsyncFunction {
  using GeneralParser = frontend::GeneralParser<ParseHandler, Unit>;

 private:
  GeneralParser* parser_;
  bool oldInParametersOfAsyncFunction_;

 public:
  AutoInParametersOfAsyncFunction(GeneralParser* parser, bool inParameters) {
    parser_ = parser;
    oldInParametersOfAsyncFunction_ = parser_->inParametersOfAsyncFunction_;
    parser_->setInParametersOfAsyncFunction(inParameters);
  }

  ~AutoInParametersOfAsyncFunction() {
    parser_->setInParametersOfAsyncFunction(oldInParametersOfAsyncFunction_);
  }
};

GlobalScope::Data* NewEmptyGlobalScopeData(JSContext* cx, LifoAlloc& alloc,
                                           uint32_t numBindings);

LexicalScope::Data* NewEmptyLexicalScopeData(JSContext* cx, LifoAlloc& alloc,
                                             uint32_t numBindings);

mozilla::Maybe<GlobalScope::Data*> NewGlobalScopeData(
    JSContext* context, ParseContext::Scope& scope, LifoAlloc& alloc,
    ParseContext* pc);
mozilla::Maybe<EvalScope::Data*> NewEvalScopeData(JSContext* context,
                                                  ParseContext::Scope& scope,
                                                  LifoAlloc& alloc,
                                                  ParseContext* pc);
mozilla::Maybe<FunctionScope::Data*> NewFunctionScopeData(
    JSContext* context, ParseContext::Scope& scope, bool hasParameterExprs,
    IsFieldInitializer isFieldInitializer, LifoAlloc& alloc, ParseContext* pc);
mozilla::Maybe<VarScope::Data*> NewVarScopeData(JSContext* context,
                                                ParseContext::Scope& scope,
                                                LifoAlloc& alloc,
                                                ParseContext* pc);
mozilla::Maybe<LexicalScope::Data*> NewLexicalScopeData(
    JSContext* context, ParseContext::Scope& scope, LifoAlloc& alloc,
    ParseContext* pc);

bool FunctionScopeHasClosedOverBindings(ParseContext* pc);
bool LexicalScopeHasClosedOverBindings(ParseContext* pc,
                                       ParseContext::Scope& scope);

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_Parser_h */
