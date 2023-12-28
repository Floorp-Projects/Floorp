/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingOperations.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include <algorithm>
#include <array>
#include <cuchar>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __cpp_lib_optional
#include <optional>
template<typename T> using optional = std::optional<T>;
#else
#include <llvm/ADT/Optional.h>
template<typename T> using optional = clang::Optional<T>;
#endif

using namespace clang;

namespace {

template<typename InputIt>
bool hasReverseQualifiedName(InputIt first, InputIt last, const NamedDecl &tag)
{
  const NamedDecl *currentDecl = &tag;
  InputIt currentName;
  for (currentName = first; currentName != last; currentName++) {
    if (!currentDecl || !currentDecl->getIdentifier() || currentDecl->getName() != *currentName)
      return false;

    currentDecl = dyn_cast<NamedDecl>(currentDecl->getDeclContext());
  }
  if (currentName != last)
    return false;

  if (currentDecl != nullptr)
    return false;

  return true;
}

bool isMozillaJniObjectBase(const CXXRecordDecl &klass)
{
  const auto qualifiedName = std::array<StringRef, 3>{"mozilla", "jni", "ObjectBase"};
  return hasReverseQualifiedName(qualifiedName.crbegin(), qualifiedName.crend(), klass);
}

bool isMozillaJniNativeImpl(const CXXRecordDecl &klass)
{
  const auto qualifiedName = std::array<StringRef, 3>{"mozilla", "jni", "NativeImpl"};
  return hasReverseQualifiedName(qualifiedName.crbegin(), qualifiedName.crend(), klass);
}

const NamedDecl *fieldNamed(StringRef name, const RecordDecl &strukt)
{
  for (const auto *decl : strukt.decls()) {
    const auto *namedDecl = dyn_cast<VarDecl>(decl);
    if (!namedDecl)
      continue;

    if (!namedDecl->getIdentifier() || namedDecl->getName() != name)
      continue;

    return namedDecl;
  }

  return {};
}

optional<StringRef> nameFieldValue(const RecordDecl &strukt)
{
  const auto *nameField = dyn_cast_or_null<VarDecl>(fieldNamed("name", strukt));
  if (!nameField)
    return {};

  const auto *def = nameField->getDefinition();
  if (!def)
    return {};

  const auto *name = dyn_cast_or_null<StringLiteral>(def->getInit());
  if (!name)
    return {};

  return name->getString();
}

struct AbstractBinding {
  // Subset of tools/analysis/BindingSlotLang
  enum class Lang {
    Cpp,
    Jvm,
  };
  static constexpr size_t LangLength = 2;
  static constexpr std::array<StringRef, LangLength> langNames = {
    "cpp",
    "jvm",
  };

  static optional<Lang> langFromString(StringRef langName)
  {
    const auto it = std::find(langNames.begin(), langNames.end(), langName);
    if (it == langNames.end())
      return {};

    return Lang(it - langNames.begin());
  }
  static StringRef stringFromLang(Lang lang)
  {
    return langNames[size_t(lang)];
  }

  // Subset of tools/analysis/BindingSlotKind
  enum class Kind {
    Class,
    Method,
    Getter,
    Setter,
    Const,
  };
  static constexpr size_t KindLength = 5;
  static constexpr std::array<StringRef, KindLength> kindNames = {
    "class",
    "method",
    "getter",
    "setter",
    "const",
  };

  static optional<Kind> kindFromString(StringRef kindName)
  {
    const auto it = std::find(kindNames.begin(), kindNames.end(), kindName);
    if (it == kindNames.end())
      return {};

    return Kind(it - kindNames.begin());
  }
  static StringRef stringFromKind(Kind kind)
  {
    return kindNames[size_t(kind)];
  }

  Lang lang;
  Kind kind;
  StringRef symbol;
};
constexpr size_t AbstractBinding::KindLength;
constexpr std::array<StringRef, AbstractBinding::KindLength> AbstractBinding::kindNames;
constexpr size_t AbstractBinding::LangLength;
constexpr std::array<StringRef, AbstractBinding::LangLength> AbstractBinding::langNames;

struct BindingTo : public AbstractBinding {
  BindingTo(AbstractBinding b) : AbstractBinding(std::move(b)) {}
  static constexpr StringRef ANNOTATION = "binding_to";
};
constexpr StringRef BindingTo::ANNOTATION;

struct BoundAs : public AbstractBinding {
  BoundAs(AbstractBinding b) : AbstractBinding(std::move(b)) {}
  static constexpr StringRef ANNOTATION = "bound_as";
};
constexpr StringRef BoundAs::ANNOTATION;

template<typename B>
void setBindingAttr(ASTContext &C, Decl &decl, B binding)
{
  // recent LLVM: CreateImplicit then setDelayedArgs
  Expr *langExpr = StringLiteral::Create(C, AbstractBinding::stringFromLang(binding.lang), StringLiteral::UTF8, false, {}, {});
  Expr *kindExpr = StringLiteral::Create(C, AbstractBinding::stringFromKind(binding.kind), StringLiteral::UTF8, false, {}, {});
  Expr *symbolExpr = StringLiteral::Create(C, binding.symbol, StringLiteral::UTF8, false, {}, {});
  auto **args = new (C, 16) Expr *[3]{langExpr, kindExpr, symbolExpr};
  auto *attr = AnnotateAttr::CreateImplicit(C, B::ANNOTATION, args, 3);
  decl.addAttr(attr);
}

optional<AbstractBinding> readBinding(const AnnotateAttr &attr)
{
  if (attr.args_size() != 3)
    return {};

  const auto *langExpr = attr.args().begin()[0];
  const auto *kindExpr = attr.args().begin()[1];
  const auto *symbolExpr = attr.args().begin()[2];
  if (!langExpr || !kindExpr || !symbolExpr)
    return {};

  const auto *langName = dyn_cast<StringLiteral>(langExpr->IgnoreUnlessSpelledInSource());
  const auto *kindName = dyn_cast<StringLiteral>(kindExpr->IgnoreUnlessSpelledInSource());
  const auto *symbol = dyn_cast<StringLiteral>(symbolExpr->IgnoreUnlessSpelledInSource());
  if (!langName || !kindName || !symbol)
    return {};

  const auto lang = AbstractBinding::langFromString(langName->getString());
  const auto kind = AbstractBinding::kindFromString(kindName->getString());

  if (!lang || !kind)
    return {};

  return AbstractBinding {
    .lang = *lang,
    .kind = *kind,
    .symbol = symbol->getString(),
  };
}

optional<BindingTo> getBindingTo(const Decl &decl)
{
  for (const auto *attr : decl.specific_attrs<AnnotateAttr>()) {
    if (attr->getAnnotation() != BindingTo::ANNOTATION)
      continue;

    const auto binding = readBinding(*attr);
    if (!binding)
      continue;

    return BindingTo{*binding};
  }
  return {};
}

// C++23: turn into generator
std::vector<BoundAs> getBoundAs(const Decl &decl)
{
  std::vector<BoundAs> found;

  for (const auto *attr : decl.specific_attrs<AnnotateAttr>()) {
    if (attr->getAnnotation() != BoundAs::ANNOTATION)
      continue;

    const auto binding = readBinding(*attr);
    if (!binding)
      continue;

    found.push_back(BoundAs{*binding});
  }

  return found;
}

class FindCallCall : private RecursiveASTVisitor<FindCallCall>
{
public:
  struct Result {
    using Kind = AbstractBinding::Kind;

    Kind kind;
    StringRef name;
  };

  static optional<Result> search(Stmt *statement)
  {
    FindCallCall finder;
    finder.TraverseStmt(statement);
    return finder.result;
  }

private:
  optional<Result> result;

  friend RecursiveASTVisitor<FindCallCall>;

  optional<Result> tryParseCallCall(CallExpr *callExpr){
    const auto *callee = dyn_cast_or_null<CXXMethodDecl>(callExpr->getDirectCallee());
    if (!callee)
      return {};

    if (!callee->getIdentifier())
      return {};

    const auto action = callee->getIdentifier()->getName();

    if (action != "Call" && action != "Get" && action != "Set")
      return {};

    const auto *parentClass = dyn_cast_or_null<ClassTemplateSpecializationDecl>(callee->getParent());

    if (!parentClass)
      return {};

    const auto *parentTemplate = parentClass->getTemplateInstantiationPattern();

    if (!parentTemplate || !parentTemplate->getIdentifier())
      return {};

    const auto parentName = parentTemplate->getIdentifier()->getName();

    AbstractBinding::Kind kind;
    if (action == "Call") {
      if (parentName == "Constructor" || parentName == "Method") {
        kind = AbstractBinding::Kind::Method;
      } else {
        return {};
      }
    } else if (parentName == "Field") {
      if (action == "Get") {
        kind = AbstractBinding::Kind::Getter;
      } else if (action == "Set") {
        kind = AbstractBinding::Kind::Setter;
      } else {
        return {};
      }
    } else {
      return {};
    }

    const auto *templateArg = parentClass->getTemplateArgs().get(0).getAsType()->getAsRecordDecl();

    if (!templateArg)
      return {};

    const auto name = nameFieldValue(*templateArg);
    if (!name)
      return {};

    return Result {
      .kind = kind,
      .name = *name,
    };

    return {};
  }
  bool VisitCallExpr(CallExpr *callExpr)
  {
    return !(result = tryParseCallCall(callExpr));
  }
};

constexpr StringRef JVM_SCIP_SYMBOL_PREFIX = "S_jvm_";

std::string javaScipSymbol(StringRef prefix, StringRef name, AbstractBinding::Kind kind)
{
  auto symbol = (prefix + name).str();

  switch (kind) {
  case AbstractBinding::Kind::Class:
    std::replace(symbol.begin(), symbol.end(), '$', '#');
    symbol += "#";
    break;
  case AbstractBinding::Kind::Method:
    symbol += "().";
    break;
  case AbstractBinding::Kind::Const:
  case AbstractBinding::Kind::Getter:
  case AbstractBinding::Kind::Setter:
    symbol += ".";
    break;
  }

  return symbol;
}

void addSlotOwnerAttribute(llvm::json::OStream &J, const Decl &decl)
{
  if (const auto bindingTo = getBindingTo(decl)) {
    J.attributeBegin("slotOwner");
    J.objectBegin();
    J.attribute("slotKind", AbstractBinding::stringFromKind(bindingTo->kind));
    J.attribute("slotLang", "cpp");
    J.attribute("ownerLang", AbstractBinding::stringFromLang(bindingTo->lang));
    J.attribute("sym", bindingTo->symbol);
    J.objectEnd();
    J.attributeEnd();
  }
}
void addBindingSlotsAttribute(llvm::json::OStream &J, const Decl &decl)
{
  const auto allBoundAs = getBoundAs(decl);
  if (!allBoundAs.empty()) {
    J.attributeBegin("bindingSlots");
    J.arrayBegin();
    for (const auto boundAs : allBoundAs) {
      J.objectBegin();
      J.attribute("slotKind", AbstractBinding::stringFromKind(boundAs.kind));
      J.attribute("slotLang", AbstractBinding::stringFromLang(boundAs.lang));
      J.attribute("ownerLang", "cpp");
      J.attribute("sym", boundAs.symbol);
      J.objectEnd();
    }
    J.arrayEnd();
    J.attributeEnd();
  }
}

// The mangling scheme is documented at https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/design.html
// The main takeaways are:
// - _0xxxx is the utf16 code unit xxxx
// - _1 is _
// - _2 is ;
// - _3 is [
// - __ is the separator between function name and overload specification
// - _ is otherwise the separator between packages/classes/methods
//
// This method takes a StringRef & and mutates it and can be called twice on a Jnicall function name to get
//  first the demangled name
//  second the demangled overload specification
// But we don't use the later for now because we have no way to map that to how SCIP resolves overloads.
optional<std::string> demangleJnicallPart(StringRef &remainder)
{
  std::string demangled;

  std::mbstate_t ps = {};

  while (!remainder.empty()) {
    switch (remainder[0]) {
      case '0': {
        remainder = remainder.drop_front(1);

        uint16_t codeUnit;
        const auto ok = remainder.substr(1, 4).getAsInteger(16, codeUnit);
        remainder = remainder.drop_front(4);

        if (!ok) // failed reading xxxx as hexadecimal from _0xxxx
          return {};

        std::array<char, MB_LEN_MAX> codePoint;
        const auto mbLen = std::c16rtomb(codePoint.data(), codeUnit, &ps);

        if (mbLen == -1) // failed converting utf16 to utf8
          return {};

        demangled += StringRef(codePoint.begin(), mbLen);
        break;
      }
      case '1':
        remainder = remainder.drop_front(1);
        ps = {};
        demangled += '_';
        break;
      case '2':
        remainder = remainder.drop_front(1);
        ps = {};
        demangled += ';';
        break;
      case '3':
        remainder = remainder.drop_front(1);
        ps = {};
        demangled += '[';
        break;
      case '_':
        remainder = remainder.drop_front(1);
        ps = {};
        if (remainder.empty()) // the string ends with _
          return {};

        switch (remainder[0]) {
          case '0':
          case '1':
          case '2':
          case '3':
            demangled += '.';
            break;
          default:
            // either:
            // * the string began with _[^0-3], which is not supposed to happen; or
            // * we reached __[^0-3] meaning we finished the first part of the name and remainder holds the overload specification
            return demangled;
        }
      default:
        ps = {};
        demangled += '.';
        break;
    }
    StringRef token;
    std::tie(token, remainder) = remainder.split('_');
    demangled += token;
  }

  return demangled;
}

optional<std::string> scipSymbolFromJnicallFunctionName(StringRef functionName)
{
  if (!functionName.consume_front("Java_"))
    return {};

  const auto demangledName = demangleJnicallPart(functionName);

  if (!demangledName || demangledName->empty())
    return {};

  // demangleJavaName returns something like .some.package.Class$InnerClass.method
  // - prepend S_jvm_
  // - remove the leading dot
  // - replace the last dot with a #
  // - replace the other dots with /
  // - replace $ with #
  // - add the ([+overloadNumber]). suffix
  auto symbol = JVM_SCIP_SYMBOL_PREFIX.str();
  symbol += demangledName->substr(1);
  const auto lastDot = symbol.rfind('.');
  if (lastDot != std::string::npos)
    symbol[lastDot] = '#';
  std::replace(symbol.begin(), symbol.end(), '.', '/');
  std::replace(symbol.begin(), symbol.end(), '$', '#');

  // Keep track of how many times we have seen this method, to build the ([+overloadNumber]). suffix.
  // This assumes this function is called on C function definitions in the same order the matching overloads are declared in Java.
  static std::unordered_map<std::string, uint> jnicallFunctions;
  auto &overloadNumber = jnicallFunctions[symbol];

  symbol += '(';
  if (overloadNumber) {
    symbol += '+';
    symbol += overloadNumber;
    overloadNumber++;
  }
  symbol += ").";

  return symbol;
};

} // anonymous namespace

// class [wrapper] : public mozilla::jni::ObjectBase<[wrapper]>
// {
//   static constexpr char name[] = "[nameFieldValue]";
// }
void findBindingToJavaClass(ASTContext &C, CXXRecordDecl &klass)
{
  for (const auto &baseSpecifier : klass.bases()) {
    const auto *base = baseSpecifier.getType()->getAsCXXRecordDecl();
    if (!base)
      continue;

    if (!isMozillaJniObjectBase(*base))
      continue;

    const auto name = nameFieldValue(klass);
    if (!name)
      continue;

    const auto symbol = javaScipSymbol(JVM_SCIP_SYMBOL_PREFIX, *name, BindingTo::Kind::Class);
    const auto binding = BindingTo {{
      .lang = BindingTo::Lang::Jvm,
      .kind = BindingTo::Kind::Class,
      .symbol = symbol,
    }};

    setBindingAttr(C, klass, binding);
    return;
  }
}

// When a Java method is marked as native, the JRE looks by default for a function
// named Java_<mangled method name>[__<mangled overload specification>].
void findBindingToJavaFunction(ASTContext &C, FunctionDecl &function)
{
  const auto *identifier = function.getIdentifier();
  if (!identifier)
    return;

  const auto name = identifier->getName();
  const auto symbol = scipSymbolFromJnicallFunctionName(name);
  if (!symbol)
    return;

  const auto binding = BoundAs {{
    .lang = BindingTo::Lang::Jvm,
    .kind = BindingTo::Kind::Method,
    .symbol = *symbol,
  }};

  setBindingAttr(C, function, binding);
}

// class [parent]
// {
//   struct [methodStruct] {
//     static constexpr char name[] = "[methodNameFieldValue]";
//   }
//   [method]
//   {
//     ...
//     mozilla::jni::{Method,Constructor,Field}<[methodStruct]>::{Call,Get,Set}(...)
//     ...
//   }
// }
void findBindingToJavaMember(ASTContext &C, CXXMethodDecl &method)
{
  const auto *parent = method.getParent();
  if (!parent)
    return;
  const auto classBinding = getBindingTo(*parent);
  if (!classBinding)
    return;

  auto *body = method.getBody();
  if (!body)
    return;

  const auto found = FindCallCall::search(body);
  if (!found)
    return;

  const auto symbol = javaScipSymbol(classBinding->symbol, found->name, found->kind);
  const auto binding = BindingTo {{
    .lang = BindingTo::Lang::Jvm,
    .kind = found->kind,
    .symbol = symbol,
  }};

  setBindingAttr(C, method, binding);
}

// class [parent]
// {
//   struct [methodStruct] {
//     static constexpr char name[] = "[methodNameFieldValue]";
//   }
//   [method]
//   {
//     ...
//     mozilla::jni::{Method,Constructor,Field}<[methodStruct]>::{Call,Get,Set}(...)
//     ...
//   }
// }
void findBindingToJavaConstant(ASTContext &C, VarDecl &field)
{
  const auto *parent = dyn_cast_or_null<CXXRecordDecl>(field.getDeclContext());
  if (!parent)
    return;

  const auto classBinding = getBindingTo(*parent);
  if (!classBinding)
    return;

  const auto symbol = javaScipSymbol(classBinding->symbol, field.getName(), BindingTo::Kind::Const);
  const auto binding = BindingTo {{
    .lang = BindingTo::Lang::Jvm,
    .kind = BindingTo::Kind::Const,
    .symbol = symbol,
  }};

  setBindingAttr(C, field, binding);
}

// class [klass] : public [wrapper]::Natives<[klass]> {...}
// class [wrapper] : public mozilla::jni::ObjectBase<[wrapper]>
// {
//   static constexpr char name[] = "[nameFieldValue]";
//
//   struct [methodStruct] {
//     static constexpr char name[] = "[methodNameFieldValue]";
//   }
//
//   template<typename T>
//   class [wrapper]::Natives : public mozilla::jni::NativeImpl<[wrapper], T> {
//     static const JNINativeMethod methods[] = {
//       mozilla::jni::MakeNativeMethod<[wrapper]::[methodStruct]>(
//               mozilla::jni::NativeStub<[wrapper]::[methodStruct], Impl>
//               ::template Wrap<&Impl::[method]>),
//     }
//   }
// }
void findBoundAsJavaClasses(ASTContext &C, CXXRecordDecl &klass)
{
  for (const auto &baseSpecifier : klass.bases()) {
    const auto *base = baseSpecifier.getType()->getAsCXXRecordDecl();
    if (!base)
      continue;

    for (const auto &baseBaseSpecifier : base->bases()) {
      const auto *baseBase = dyn_cast_or_null<ClassTemplateSpecializationDecl>(baseBaseSpecifier.getType()->getAsCXXRecordDecl());
      if (!baseBase)
        continue;

      if (!isMozillaJniNativeImpl(*baseBase))
        continue;

      const auto *wrapper = baseBase->getTemplateArgs().get(0).getAsType()->getAsCXXRecordDecl();

      if (!wrapper)
        continue;

      const auto name = nameFieldValue(*wrapper);
      if (!name)
        continue;

      const auto javaClassSymbol = javaScipSymbol(JVM_SCIP_SYMBOL_PREFIX, *name, BoundAs::Kind::Class);
      const auto classBinding = BoundAs {{
        .lang = BoundAs::Lang::Jvm,
        .kind = BoundAs::Kind::Class,
        .symbol = javaClassSymbol,
      }};
      setBindingAttr(C, klass, classBinding);

      const auto *methodsDecl = dyn_cast_or_null<VarDecl>(fieldNamed("methods", *base));
      if (!methodsDecl)
        continue;

      const auto *methodsDef = methodsDecl->getDefinition();
      if (!methodsDef)
        continue;

      const auto *inits = dyn_cast_or_null<InitListExpr>(methodsDef->getInit());
      if (!inits)
        continue;

      std::set<const CXXMethodDecl *> alreadyBound;

      for (const auto *init : inits->inits()) {
        const auto *call = dyn_cast<CallExpr>(init->IgnoreUnlessSpelledInSource());
        if (!call)
          continue;

        const auto *funcDecl = call->getDirectCallee();
        if (!funcDecl)
          continue;

        const auto *templateArgs = funcDecl->getTemplateSpecializationArgs();
        if (!templateArgs)
          continue;

        const auto *strukt = dyn_cast_or_null<RecordDecl>(templateArgs->get(0).getAsType()->getAsRecordDecl());
        if (!strukt)
          continue;

        const auto *wrapperRef = dyn_cast_or_null<DeclRefExpr>(call->getArg(0)->IgnoreUnlessSpelledInSource());
        if (!wrapperRef)
          continue;

        const auto *boundRef = dyn_cast_or_null<UnaryOperator>(wrapperRef->template_arguments().front().getArgument().getAsExpr());
        if (!boundRef)
          continue;

        auto addToBound = [&](CXXMethodDecl &boundDecl, uint overloadNum) {
          const auto methodName = nameFieldValue(*strukt);
          if (!methodName)
            return;

          auto javaMethodSymbol = javaClassSymbol;
          javaMethodSymbol += *methodName;
          javaMethodSymbol += '(';
          if (overloadNum > 0) {
            javaMethodSymbol += '+';
            javaMethodSymbol += std::to_string(overloadNum);
          }
          javaMethodSymbol += ").";

          const auto binding = BoundAs {{
            .lang = BoundAs::Lang::Jvm,
            .kind = BoundAs::Kind::Method,
            .symbol = javaMethodSymbol,
          }};
          setBindingAttr(C, boundDecl, binding);
        };

        if (auto *bound = dyn_cast_or_null<DeclRefExpr>(boundRef->getSubExpr())) {
          auto *method = dyn_cast_or_null<CXXMethodDecl>(bound->getDecl());
          if (!method)
            continue;
          addToBound(*method, 0);
        } else if (const auto *bound = dyn_cast_or_null<UnresolvedLookupExpr>(boundRef->getSubExpr())) {
          // XXX This is hackish
          // In case of overloads it's not obvious which one we should use
          // this expects the declaration order between C++ and Java to match
          auto declarations = std::vector<Decl*>(bound->decls_begin(), bound->decls_end());
          auto byLocation = [](Decl *a, Decl *b){ return a->getLocation() < b->getLocation(); };
          std::sort(declarations.begin(), declarations.end(), byLocation);

          uint i = 0;
          for (auto *decl : declarations) {
            auto *method = dyn_cast<CXXMethodDecl>(decl);
            if (!method)
              continue;
            if (alreadyBound.find(method) == alreadyBound.end()) {
              addToBound(*method, i);
              alreadyBound.insert(method);
              break;
            }
            i++;
          }
        }
      }
    }
  }
}

void emitBindingAttributes(llvm::json::OStream &J, const Decl &decl)
{
  addSlotOwnerAttribute(J, decl);
  addBindingSlotsAttribute(J, decl);
}
