typedef union {
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
#define	INVARIANT	258
#define	HIGH_PRECISION	259
#define	MEDIUM_PRECISION	260
#define	LOW_PRECISION	261
#define	PRECISION	262
#define	ATTRIBUTE	263
#define	CONST_QUAL	264
#define	BOOL_TYPE	265
#define	FLOAT_TYPE	266
#define	INT_TYPE	267
#define	BREAK	268
#define	CONTINUE	269
#define	DO	270
#define	ELSE	271
#define	FOR	272
#define	IF	273
#define	DISCARD	274
#define	RETURN	275
#define	BVEC2	276
#define	BVEC3	277
#define	BVEC4	278
#define	IVEC2	279
#define	IVEC3	280
#define	IVEC4	281
#define	VEC2	282
#define	VEC3	283
#define	VEC4	284
#define	MATRIX2	285
#define	MATRIX3	286
#define	MATRIX4	287
#define	IN_QUAL	288
#define	OUT_QUAL	289
#define	INOUT_QUAL	290
#define	UNIFORM	291
#define	VARYING	292
#define	STRUCT	293
#define	VOID_TYPE	294
#define	WHILE	295
#define	SAMPLER2D	296
#define	SAMPLERCUBE	297
#define	IDENTIFIER	298
#define	TYPE_NAME	299
#define	FLOATCONSTANT	300
#define	INTCONSTANT	301
#define	BOOLCONSTANT	302
#define	FIELD_SELECTION	303
#define	LEFT_OP	304
#define	RIGHT_OP	305
#define	INC_OP	306
#define	DEC_OP	307
#define	LE_OP	308
#define	GE_OP	309
#define	EQ_OP	310
#define	NE_OP	311
#define	AND_OP	312
#define	OR_OP	313
#define	XOR_OP	314
#define	MUL_ASSIGN	315
#define	DIV_ASSIGN	316
#define	ADD_ASSIGN	317
#define	MOD_ASSIGN	318
#define	LEFT_ASSIGN	319
#define	RIGHT_ASSIGN	320
#define	AND_ASSIGN	321
#define	XOR_ASSIGN	322
#define	OR_ASSIGN	323
#define	SUB_ASSIGN	324
#define	LEFT_PAREN	325
#define	RIGHT_PAREN	326
#define	LEFT_BRACKET	327
#define	RIGHT_BRACKET	328
#define	LEFT_BRACE	329
#define	RIGHT_BRACE	330
#define	DOT	331
#define	COMMA	332
#define	COLON	333
#define	EQUAL	334
#define	SEMICOLON	335
#define	BANG	336
#define	DASH	337
#define	TILDE	338
#define	PLUS	339
#define	STAR	340
#define	SLASH	341
#define	PERCENT	342
#define	LEFT_ANGLE	343
#define	RIGHT_ANGLE	344
#define	VERTICAL_BAR	345
#define	CARET	346
#define	AMPERSAND	347
#define	QUESTION	348

