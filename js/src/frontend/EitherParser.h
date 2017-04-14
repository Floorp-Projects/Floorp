/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A variant-like class abstracting operations on a Parser with a given ParseHandler but
 * unspecified character type.
 */

#ifndef frontend_EitherParser_h
#define frontend_EitherParser_h

#include "mozilla/Attributes.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Move.h"
#include "mozilla/Tuple.h"
#include "mozilla/Variant.h"

#include "frontend/Parser.h"
#include "frontend/TokenStream.h"

namespace js {
namespace frontend {

template<template<typename CharT> class ParseHandler>
class EitherParser
{
    const mozilla::Variant<Parser<ParseHandler, char16_t>* const> parser;

    using Node = typename ParseHandler<char16_t>::Node;

  public:
    template<class Parser>
    explicit EitherParser(Parser* parser) : parser(parser) {}

  private:
    struct TokenStreamMatcher
    {
        template<class Parser>
        TokenStreamAnyChars& match(Parser* parser) {
            return parser->tokenStream;
        }
    };

  public:
    TokenStreamAnyChars& tokenStream() {
        return parser.match(TokenStreamMatcher());
    }

    const TokenStreamAnyChars& tokenStream() const {
        return parser.match(TokenStreamMatcher());
    }

  private:
    struct ScriptSourceMatcher
    {
        template<class Parser>
        ScriptSource* match(Parser* parser) {
            return parser->ss;
        }
    };

  public:
    ScriptSource* ss() {
        return parser.match(ScriptSourceMatcher());
    }

  private:
    struct OptionsMatcher
    {
        template<class Parser>
        const JS::ReadOnlyCompileOptions& match(Parser* parser) {
            return parser->options();
        }
    };

  public:
    const JS::ReadOnlyCompileOptions& options() {
        return parser.match(OptionsMatcher());
    }

  private:
    struct ComputeErrorMetadataMatcher
    {
        ErrorMetadata* metadata;
        uint32_t offset;

        ComputeErrorMetadataMatcher(ErrorMetadata* metadata, uint32_t offset)
          : metadata(metadata), offset(offset)
        {}

        template<class Parser>
        MOZ_MUST_USE bool match(Parser* parser) {
            return parser->tokenStream.computeErrorMetadata(metadata, offset);
        }
    };

  public:
    MOZ_MUST_USE bool computeErrorMetadata(ErrorMetadata* metadata, uint32_t offset) {
        return parser.match(ComputeErrorMetadataMatcher(metadata, offset));
    }

  private:
    struct NewObjectBoxMatcher
    {
        JSObject* obj;

        explicit NewObjectBoxMatcher(JSObject* obj) : obj(obj) {}

        template<class Parser>
        ObjectBox* match(Parser* parser) {
            return parser->newObjectBox(obj);
        }
    };

  public:
    ObjectBox* newObjectBox(JSObject* obj) {
        return parser.match(NewObjectBoxMatcher(obj));
    }

  private:
    struct SingleBindingFromDeclarationMatcher
    {
        Node decl;

        explicit SingleBindingFromDeclarationMatcher(Node decl) : decl(decl) {}

        template<class Parser>
        Node match(Parser* parser) {
            return parser->handler.singleBindingFromDeclaration(decl);
        }
    };

  public:
    Node singleBindingFromDeclaration(Node decl) {
        return parser.match(SingleBindingFromDeclarationMatcher(decl));
    }

  private:
    struct IsDeclarationListMatcher
    {
        Node node;

        explicit IsDeclarationListMatcher(Node node) : node(node) {}

        template<class Parser>
        bool match(Parser* parser) {
            return parser->handler.isDeclarationList(node);
        }
    };

  public:
    bool isDeclarationList(Node node) {
        return parser.match(IsDeclarationListMatcher(node));
    }

  private:
    struct IsSuperBaseMatcher
    {
        Node node;

        explicit IsSuperBaseMatcher(Node node) : node(node) {}

        template<class Parser>
        bool match(Parser* parser) {
            return parser->handler.isSuperBase(node);
        }
    };

  public:
    bool isSuperBase(Node node) {
        return parser.match(IsSuperBaseMatcher(node));
    }

  private:
    template<typename Function, class This, typename... Args, size_t... Indices>
    static auto
    CallGenericFunction(Function func, This* obj,
                        mozilla::Tuple<Args...>& args, mozilla::IndexSequence<Indices...>)
      -> decltype(((*obj).*func)(mozilla::Get<Indices>(args)...))
    {
        return ((*obj).*func)(mozilla::Get<Indices>(args)...);
    }

    template<typename... StoredArgs>
    struct ReportErrorMatcher
    {
        mozilla::Tuple<StoredArgs...> args;

        template<typename... Args>
        explicit ReportErrorMatcher(Args&&... actualArgs)
          : args { mozilla::Forward<Args>(actualArgs)... }
        {}

        template<class Parser>
        void match(Parser* parser) {
            return CallGenericFunction(&TokenStream::reportError,
                                       &parser->tokenStream,
                                       args,
                                       typename mozilla::IndexSequenceFor<StoredArgs...>::Type());
        }
    };

  public:
    template<typename... Args>
    void reportError(Args&&... args) {
        ReportErrorMatcher<typename mozilla::Decay<Args>::Type...>
            matcher { mozilla::Forward<Args>(args)... };
        return parser.match(mozilla::Move(matcher));
    }

  private:
    struct ParserBaseMatcher
    {
        template<class Parser>
        ParserBase& match(Parser* parser) {
            return *parser;
        }
    };

  public:
    template<typename... Args>
    MOZ_MUST_USE bool reportNoOffset(Args&&... args) {
        return parser.match(ParserBaseMatcher()).reportNoOffset(mozilla::Forward<Args>(args)...);
    }

  private:
    template<typename... StoredArgs>
    struct ReportExtraWarningMatcher
    {
        mozilla::Tuple<StoredArgs...> args;

        template<typename... Args>
        explicit ReportExtraWarningMatcher(Args&&... actualArgs)
          : args { mozilla::Forward<Args>(actualArgs)... }
        {}

        template<class Parser>
        MOZ_MUST_USE bool match(Parser* parser) {
            return CallGenericFunction(&TokenStream::reportExtraWarningErrorNumberVA,
                                       &parser->tokenStream,
                                       args,
                                       typename mozilla::IndexSequenceFor<StoredArgs...>::Type());
        }
    };

  public:
    template<typename... Args>
    MOZ_MUST_USE bool reportExtraWarningErrorNumberVA(Args&&... args) {
        ReportExtraWarningMatcher<typename mozilla::Decay<Args>::Type...>
            matcher { mozilla::Forward<Args>(args)... };
        return parser.match(mozilla::Move(matcher));
    }

  private:
    template<typename... StoredArgs>
    struct ReportStrictModeErrorMatcher
    {
        mozilla::Tuple<StoredArgs...> args;

        template<typename... Args>
        explicit ReportStrictModeErrorMatcher(Args&&... actualArgs)
          : args { mozilla::Forward<Args>(actualArgs)... }
        {}

        template<class Parser>
        MOZ_MUST_USE bool match(Parser* parser) {
            return CallGenericFunction(&TokenStream::reportStrictModeErrorNumberVA,
                                       &parser->tokenStream,
                                       args,
                                       typename mozilla::IndexSequenceFor<StoredArgs...>::Type());
        }
    };

  public:
    template<typename... Args>
    MOZ_MUST_USE bool reportStrictModeErrorNumberVA(Args&&... args) {
        ReportStrictModeErrorMatcher<typename mozilla::Decay<Args>::Type...>
            matcher { mozilla::Forward<Args>(args)... };
        return parser.match(mozilla::Move(matcher));
    }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_EitherParser_h */
