
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     INVARIANT = 258,
     HIGH_PRECISION = 259,
     MEDIUM_PRECISION = 260,
     LOW_PRECISION = 261,
     PRECISION = 262,
     ATTRIBUTE = 263,
     CONST_QUAL = 264,
     BOOL_TYPE = 265,
     FLOAT_TYPE = 266,
     INT_TYPE = 267,
     BREAK = 268,
     CONTINUE = 269,
     DO = 270,
     ELSE = 271,
     FOR = 272,
     IF = 273,
     DISCARD = 274,
     RETURN = 275,
     BVEC2 = 276,
     BVEC3 = 277,
     BVEC4 = 278,
     IVEC2 = 279,
     IVEC3 = 280,
     IVEC4 = 281,
     VEC2 = 282,
     VEC3 = 283,
     VEC4 = 284,
     MATRIX2 = 285,
     MATRIX3 = 286,
     MATRIX4 = 287,
     IN_QUAL = 288,
     OUT_QUAL = 289,
     INOUT_QUAL = 290,
     UNIFORM = 291,
     VARYING = 292,
     STRUCT = 293,
     VOID_TYPE = 294,
     WHILE = 295,
     SAMPLER2D = 296,
     SAMPLERCUBE = 297,
     IDENTIFIER = 298,
     TYPE_NAME = 299,
     FLOATCONSTANT = 300,
     INTCONSTANT = 301,
     BOOLCONSTANT = 302,
     FIELD_SELECTION = 303,
     LEFT_OP = 304,
     RIGHT_OP = 305,
     INC_OP = 306,
     DEC_OP = 307,
     LE_OP = 308,
     GE_OP = 309,
     EQ_OP = 310,
     NE_OP = 311,
     AND_OP = 312,
     OR_OP = 313,
     XOR_OP = 314,
     MUL_ASSIGN = 315,
     DIV_ASSIGN = 316,
     ADD_ASSIGN = 317,
     MOD_ASSIGN = 318,
     LEFT_ASSIGN = 319,
     RIGHT_ASSIGN = 320,
     AND_ASSIGN = 321,
     XOR_ASSIGN = 322,
     OR_ASSIGN = 323,
     SUB_ASSIGN = 324,
     LEFT_PAREN = 325,
     RIGHT_PAREN = 326,
     LEFT_BRACKET = 327,
     RIGHT_BRACKET = 328,
     LEFT_BRACE = 329,
     RIGHT_BRACE = 330,
     DOT = 331,
     COMMA = 332,
     COLON = 333,
     EQUAL = 334,
     SEMICOLON = 335,
     BANG = 336,
     DASH = 337,
     TILDE = 338,
     PLUS = 339,
     STAR = 340,
     SLASH = 341,
     PERCENT = 342,
     LEFT_ANGLE = 343,
     RIGHT_ANGLE = 344,
     VERTICAL_BAR = 345,
     CARET = 346,
     AMPERSAND = 347,
     QUESTION = 348
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{


    struct {
        TSourceLoc line;
        union {
            TString *string;
            float f;
            int i;
            bool b;
        };
        TSymbol* symbol;
    } lex;
    struct {
        TSourceLoc line;
        TOperator op;
        union {
            TIntermNode* intermNode;
            TIntermNodePair nodePair;
            TIntermTyped* intermTypedNode;
            TIntermAggregate* intermAggregate;
        };
        union {
            TPublicType type;
            TPrecision precision;
            TQualifier qualifier;
            TFunction* function;
            TParameter param;
            TTypeLine typeLine;
            TTypeList* typeList;
        };
    } interm;



} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




