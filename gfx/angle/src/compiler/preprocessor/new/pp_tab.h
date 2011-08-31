/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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
     HASH = 258,
     HASH_DEFINE_OBJ = 259,
     HASH_DEFINE_FUNC = 260,
     HASH_UNDEF = 261,
     HASH_IF = 262,
     HASH_IFDEF = 263,
     HASH_IFNDEF = 264,
     HASH_ELSE = 265,
     HASH_ELIF = 266,
     HASH_ENDIF = 267,
     DEFINED = 268,
     HASH_ERROR = 269,
     HASH_PRAGMA = 270,
     HASH_EXTENSION = 271,
     HASH_VERSION = 272,
     HASH_LINE = 273,
     SPACE = 274,
     INT_CONSTANT = 275,
     FLOAT_CONSTANT = 276,
     IDENTIFIER = 277
   };
#endif
/* Tokens.  */
#define HASH 258
#define HASH_DEFINE_OBJ 259
#define HASH_DEFINE_FUNC 260
#define HASH_UNDEF 261
#define HASH_IF 262
#define HASH_IFDEF 263
#define HASH_IFNDEF 264
#define HASH_ELSE 265
#define HASH_ELIF 266
#define HASH_ENDIF 267
#define DEFINED 268
#define HASH_ERROR 269
#define HASH_PRAGMA 270
#define HASH_EXTENSION 271
#define HASH_VERSION 272
#define HASH_LINE 273
#define SPACE 274
#define INT_CONSTANT 275
#define FLOAT_CONSTANT 276
#define IDENTIFIER 277




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE

{
    int ival;
    std::string* sval;
    std::vector<std::string*>* slist;
    pp::Token* tval;
    pp::TokenVector* tlist;
}
/* Line 1489 of yacc.c.  */

	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


