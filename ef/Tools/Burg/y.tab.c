#ifndef lint
char yysccsid[] = "@(#)yaccpar	1.4 (Berkeley) 02/25/90";
#endif
char rcsid_gram[] = "$Id: y.tab.c,v 1.1 1999/01/28 08:21:57 fur%netscape.com Exp $";

#include <stdio.h>
#include "b.h"
#include "fe.h"
typedef union {
	int y_int;
	char *y_string;
	Arity y_arity;
	Binding y_binding;
	PatternAST y_patternAST;
	RuleAST y_ruleAST;
	List y_list;
	IntList y_intlist;
} YYSTYPE;
#define ERROR 257
#define K_TERM 258
#define K_START 259
#define K_PPERCENT 260
#define INT 261
#define ID 262
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,   10,    7,    7,    1,    1,    9,    9,    2,
    8,    8,    5,    6,    6,    6,    3,    3,    4,    4,
    4,
};
short yylen[] = {                                         2,
    1,    2,    3,    0,    2,    2,    2,    0,    2,    3,
    0,    2,    7,    1,    4,    6,    0,    4,    0,    3,
    2,
};
short yydefred[] = {                                      4,
    0,    0,    0,    8,    0,   11,    5,    2,    0,    7,
    0,    0,    9,    0,   12,    0,    0,   10,    0,    0,
    0,    0,    0,    0,   15,    0,    0,    0,    0,    0,
   13,   16,    0,    0,    0,   21,    0,   18,   20,
};
short yydgoto[] = {                                       1,
    7,   13,   28,   35,   15,   20,    2,   11,    9,    3,
};
short yysindex[] = {                                      0,
    0, -247, -251,    0, -245,    0,    0,    0, -243,    0,
 -242,  -43,    0,  -35,    0, -240, -238,    0,  -15,  -34,
 -238, -235,  -37,  -12,    0, -238, -232,  -29,  -10,  -44,
    0,    0,  -44, -229,   -8,    0,  -44,    0,    0,
};
short yyrindex[] = {                                      0,
    0,    0,   34,    0,    0,    0,    0,    0, -244,    0,
    1,    0,    0,    0,    0,    0,    0,    0,  -39,    0,
    0,    0,    0,  -24,    0,    0,    0,    0,    0,   -5,
    0,    0,   -5,    0,    0,    0,   -5,    0,    0,
};
short yygindex[] = {                                      0,
    0,    0,    0,  -27,    0,  -18,    0,    0,    0,    0,
};
#define YYTABLESIZE 261
short yytable[] = {                                      34,
    3,   14,   23,   25,   14,   36,   26,   29,    8,   39,
    4,    5,    6,    6,    6,    6,   10,   16,   12,   14,
   18,   14,   17,   19,   21,   24,   22,   27,   30,   31,
   32,   37,   38,    1,   17,   19,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   33,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    3,
};
short yycheck[] = {                                      44,
    0,   41,   21,   41,   44,   33,   44,   26,  260,   37,
  258,  259,  260,  258,  259,  260,  262,   61,  262,  262,
  261,   61,   58,  262,   40,  261,   61,   40,  261,   59,
   41,  261,   41,    0,   59,   41,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  261,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  260,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 262
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'('","')'",0,0,"','",0,0,0,0,0,0,0,0,0,0,0,0,0,"':'","';'",0,"'='",
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"ERROR",
"K_TERM","K_START","K_PPERCENT","INT","ID",
};
char *yyrule[] = {
"$accept : full",
"full : spec",
"full : spec K_PPERCENT",
"spec : decls K_PPERCENT rules",
"decls :",
"decls : decls decl",
"decl : K_TERM bindinglist",
"decl : K_START ID",
"bindinglist :",
"bindinglist : bindinglist binding",
"binding : ID '=' INT",
"rules :",
"rules : rules rule",
"rule : ID ':' pattern '=' INT cost ';'",
"pattern : ID",
"pattern : ID '(' pattern ')'",
"pattern : ID '(' pattern ',' pattern ')'",
"cost :",
"cost : '(' INT costtail ')'",
"costtail :",
"costtail : ',' INT costtail",
"costtail : INT costtail",
};
#endif
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#ifndef YYSTACKSIZE
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 300
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
#define yystacksize YYSTACKSIZE
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define YYABORT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, reading %d (%s)\n", yystate,
                    yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: state %d, shifting to state %d\n",
                    yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: state %d, error recovery shifting\
 to state %d\n", *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: error recovery discarding state %d\n",
                            *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, error recovery discards token %d (%s)\n",
                    yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("yydebug: state %d, reducing by rule %d (%s)\n",
                yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 2:
{ yyfinished(); }
break;
case 3:
 { doSpec(yyvsp[-2].y_list , yyvsp[0].y_list ); }
break;
case 4:
 { yyval.y_list  = 0; }
break;
case 5:
 { yyval.y_list  = newList(yyvsp[0].y_arity , yyvsp[-1].y_list ); }
break;
case 6:
 { yyval.y_arity  = newArity(-1, yyvsp[0].y_list ); }
break;
case 7:
 { yyval.y_arity  = 0; doStart(yyvsp[0].y_string ); }
break;
case 8:
 { yyval.y_list  = 0; }
break;
case 9:
 { yyval.y_list  = newList(yyvsp[0].y_binding , yyvsp[-1].y_list ); }
break;
case 10:
 { yyval.y_binding  = newBinding(yyvsp[-2].y_string , yyvsp[0].y_int ); }
break;
case 11:
 { yyval.y_list  = 0; }
break;
case 12:
 { yyval.y_list  = newList(yyvsp[0].y_ruleAST , yyvsp[-1].y_list ); }
break;
case 13:
 { yyval.y_ruleAST  = newRuleAST(yyvsp[-6].y_string , yyvsp[-4].y_patternAST , yyvsp[-2].y_int , yyvsp[-1].y_intlist ); }
break;
case 14:
 { yyval.y_patternAST  = newPatternAST(yyvsp[0].y_string , 0); }
break;
case 15:
 { yyval.y_patternAST  = newPatternAST(yyvsp[-3].y_string , newList(yyvsp[-1].y_patternAST ,0)); }
break;
case 16:
 { yyval.y_patternAST  = newPatternAST(yyvsp[-5].y_string , newList(yyvsp[-3].y_patternAST , newList(yyvsp[-1].y_patternAST , 0))); }
break;
case 17:
 { yyval.y_intlist  = 0; }
break;
case 18:
 { yyval.y_intlist  = newIntList(yyvsp[-2].y_int , yyvsp[-1].y_intlist ); }
break;
case 19:
 { yyval.y_intlist  = 0; }
break;
case 20:
 { yyval.y_intlist  = newIntList(yyvsp[-1].y_int , yyvsp[0].y_intlist ); }
break;
case 21:
 { yyval.y_intlist  = newIntList(yyvsp[-1].y_int , yyvsp[0].y_intlist ); }
break;
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#ifdef YYDEBUG
        if (yydebug)
            printf("yydebug: after reduction, shifting from state 0 to\
 state %d\n", YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("yydebug: state %d, reading %d (%s)\n",
                        YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#ifdef YYDEBUG
    if (yydebug)
        printf("yydebug: after reduction, shifting from state %d \
to state %d\n", *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
