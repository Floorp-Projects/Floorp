//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_CONTEXT_H_
#define COMPILER_PREPROCESSOR_CONTEXT_H_

#include <map>

#include "common/angleutils.h"
#include "Macro.h"
#include "Token.h"

struct YYLTYPE;
union YYSTYPE;

namespace pp
{

class Lexer;

class Context
{
  public:
    Context();
    ~Context();

    bool process(int count, const char* const string[], const int length[],
                 TokenVector* output);

    TokenVector* output() { return mOutput; }

    int lex(YYSTYPE* lvalp, YYLTYPE* llocp);

    bool defineMacro(pp::Token::Location location,
                     pp::Macro::Type type,
                     std::string* name,
                     pp::TokenVector* parameters,
                     pp::TokenVector* replacements);
    bool undefineMacro(const std::string* name);
    bool isMacroDefined(const std::string* name);

  private:
    DISALLOW_COPY_AND_ASSIGN(Context);
    typedef std::map<std::string, Macro*> MacroSet;

    void reset();
    void defineBuiltInMacro(const std::string& name, int value);
    bool parse();

    std::auto_ptr<Lexer> mLexer;
    TokenVector* mOutput;
    MacroSet mMacros;  // Defined macros.
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_CONTEXT_H_

