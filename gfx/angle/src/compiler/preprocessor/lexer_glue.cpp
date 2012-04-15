//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <stdlib.h>
#include <string.h>

extern "C" {
#include "compiler/preprocessor/lexer_glue.h"
#include "compiler/preprocessor/slglobals.h"
#include "compiler/preprocessor/scanner.h"
}
#include "compiler/preprocessor/new/Lexer.h"
#include "compiler/preprocessor/new/Token.h"

struct InputSrcLexer
{
    InputSrc base;
    pp::Lexer* lexer;
};

static bool CopySymbolName(const std::string& name, yystypepp* yylvalpp)
{
    if (name.length() > MAX_SYMBOL_NAME_LEN)
    {
        CPPErrorToInfoLog("BUFFER OVERFLOW");
        return false;
    }
    strcpy(yylvalpp->symbol_name, name.c_str());
    return true;
}

static int lex(InputSrc* in, yystypepp* yylvalpp)
{
    InputSrcLexer* src = ((InputSrcLexer *)in);

    pp::Token token;
    int ret = src->lexer->lex(&token);
    switch (ret)
    {
    case 0:  // EOF
        delete src->lexer;
        free(src);
        cpp->currentInput = 0;
        ret = EOF;
        break;
    case pp::Token::IDENTIFIER:
        if (CopySymbolName(token.value, yylvalpp))
        {
            yylvalpp->sc_ident = LookUpAddString(atable, token.value.c_str());
        }
        ret = CPP_IDENTIFIER;
        break;
    case pp::Token::CONST_INT:
        if (CopySymbolName(token.value, yylvalpp))
        {
            yylvalpp->sc_int = atoi(token.value.c_str());
        }
        ret = CPP_INTCONSTANT;
        break;
    case pp::Token::CONST_FLOAT:
        CopySymbolName(token.value, yylvalpp);
        ret = CPP_FLOATCONSTANT;
        break;
    case pp::Token::OP_INC:
        ret = CPP_INC_OP;
        break;
    case pp::Token::OP_DEC:
        ret = CPP_DEC_OP;
        break;
    case pp::Token::OP_RIGHT:
        ret = CPP_RIGHT_OP;
        break;
    case pp::Token::OP_LE:
        ret = CPP_LE_OP;
        break;
    case pp::Token::OP_GE:
        ret = CPP_GE_OP;
        break;
    case pp::Token::OP_EQ:
        ret = CPP_EQ_OP;
        break;
    case pp::Token::OP_NE:
        ret = CPP_NE_OP;
        break;
    case pp::Token::OP_AND:
        ret = CPP_AND_OP;
        break;
    case pp::Token::OP_XOR:
        ret = CPP_XOR_OP;
        break;
    case pp::Token::OP_OR:
        ret = CPP_OR_OP;
        break;
    case pp::Token::OP_ADD_ASSIGN:
        ret = CPP_ADD_ASSIGN;
        break;
    case pp::Token::OP_SUB_ASSIGN:
        ret = CPP_SUB_ASSIGN;
        break;
    case pp::Token::OP_MUL_ASSIGN:
        ret = CPP_MUL_ASSIGN;
        break;
    case pp::Token::OP_DIV_ASSIGN:
        ret = CPP_DIV_ASSIGN;
        break;
    case pp::Token::OP_MOD_ASSIGN:
        ret = CPP_MOD_ASSIGN;
        break;
    case pp::Token::OP_LEFT_ASSIGN:
        ret = CPP_LEFT_ASSIGN;
        break;
    case pp::Token::OP_RIGHT_ASSIGN:
        ret = CPP_RIGHT_ASSIGN;
        break;
    case pp::Token::OP_AND_ASSIGN:
        ret = CPP_AND_ASSIGN;
        break;
    case pp::Token::OP_XOR_ASSIGN:
        ret = CPP_XOR_ASSIGN;
        break;
    case pp::Token::OP_OR_ASSIGN:
        ret = CPP_OR_ASSIGN;
        break;
    default:
        break;
    }
    SetLineNumber(token.location.line);
    SetStringNumber(token.location.string);
    return ret;
}

InputSrc* LexerInputSrc(int count, const char* const string[], const int length[])
{
    pp::Lexer* lexer = new pp::Lexer;
    if (!lexer->init(count, string, length))
    {
        delete lexer;
        return 0;
    }

    InputSrcLexer* in = (InputSrcLexer *) malloc(sizeof(InputSrcLexer));
    memset(in, 0, sizeof(InputSrcLexer));
    in->base.line = 1;
    in->base.scan = lex;
    in->lexer = lexer;

    return &in->base;
}
