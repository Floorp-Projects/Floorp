#define ERROR 257
#define K_TERM 258
#define K_START 259
#define K_PPERCENT 260
#define INT 261
#define ID 262
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
extern YYSTYPE yylval;
