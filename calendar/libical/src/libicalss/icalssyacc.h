#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union {
	char* v_string;
} yystype;
# define YYSTYPE yystype
#endif
# define	STRING	257
# define	SELECT	258
# define	FROM	259
# define	WHERE	260
# define	COMMA	261
# define	QUOTE	262
# define	EQUALS	263
# define	NOTEQUALS	264
# define	LESS	265
# define	GREATER	266
# define	LESSEQUALS	267
# define	GREATEREQUALS	268
# define	AND	269
# define	OR	270
# define	EOL	271
# define	END	272
# define	IS	273
# define	NOT	274
# define	SQLNULL	275


#endif /* not BISON_Y_TAB_H */
