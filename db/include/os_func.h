/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)os_func.h	10.8 (Sleepycat) 4/19/98
 */

/* Calls which can be replaced by the application. */
struct __db_jumptab {
	int	(*j_close) __P((int));			/* DB_FUNC_CLOSE */
	void	(*j_dirfree) __P((char **, int));	/* DB_FUNC_DIRFREE */
	int	(*j_dirlist)				/* DB_FUNC_DIRLIST */
		    __P((const char *, char ***, int *));
	int	(*j_exists)				/* DB_FUNC_EXISTS */
		    __P((const char *, int *));
	void	(*j_free) __P((void *));		/* DB_FUNC_FREE */
	int	(*j_fsync) __P((int));			/* DB_FUNC_FSYNC */
	int	(*j_ioinfo) __P((const char *,		/* DB_FUNC_IOINFO */
		    int, u_int32_t *, u_int32_t *, u_int32_t *));
	void   *(*j_malloc) __P((size_t));		/* DB_FUNC_MALLOC */
	int	(*j_map)				/* DB_FUNC_MAP */
		    __P((char *, int, size_t, int, int, int, void **));
	int	(*j_open)				/* DB_FUNC_OPEN */
		    __P((const char *, int, ...));
	ssize_t	(*j_read) __P((int, void *, size_t));	/* DB_FUNC_READ */
	void   *(*j_realloc) __P((void *, size_t));	/* DB_FUNC_REALLOC */
	int	(*j_runlink) __P((char *));		/* DB_FUNC_RUNLINK */
	int	(*j_seek)				/* DB_FUNC_SEEK */
		    __P((int, size_t, db_pgno_t, u_int32_t, int, int));
	int	(*j_sleep) __P((u_long, u_long));	/* DB_FUNC_SLEEP */
	int	(*j_unlink) __P((const char *));	/* DB_FUNC_UNLINK */
	int	(*j_unmap) __P((void *, size_t));	/* DB_FUNC_UNMAP */
	ssize_t	(*j_write)				/* DB_FUNC_WRITE */
		    __P((int, const void *, size_t));
	int	(*j_yield) __P((void));			/* DB_FUNC_YIELD */
};

extern struct __db_jumptab __db_jump;

/*
 * Names used by DB to call through the jump table.
 *
 * The naming scheme goes like this: if the functionality the application can
 * replace is the same as the DB functionality, e.g., malloc, or dirlist, then
 * we use the name __db_XXX, and the application is expected to replace the
 * complete functionality, which may or may not map directly to an ANSI C or
 * POSIX 1003.1 interface.  If the functionality that the aplication replaces
 * only underlies what the DB os directory exports to other parts of DB, e.g.,
 * read, then the name __os_XXX is used, and the application can only replace
 * the underlying functionality.  Under most circumstances, the os directory
 * part of DB is the only code that should use the __os_XXX names, all other
 * parts of DB should be calling __db_XXX functions.
 */
#define	__os_close	__db_jump.j_close	/* __db_close is a wrapper. */
#define	__db_dirfree	__db_jump.j_dirfree
#define	__db_dirlist	__db_jump.j_dirlist
#define	__db_exists	__db_jump.j_exists
#define	__db_free	__db_jump.j_free
#define	__os_fsync	__db_jump.j_fsync	/* __db_fsync is a wrapper. */
#define	__db_ioinfo	__db_jump.j_ioinfo
#define	__os_open	__db_jump.j_open	/* __db_open is a wrapper. */
#define	__os_read	__db_jump.j_read	/* __db_read is a wrapper. */
#define	__db_seek	__db_jump.j_seek
#define	__db_sleep	__db_jump.j_sleep
#define	__os_unlink	__db_jump.j_unlink	/* __db_unlink is a wrapper. */
#define	__os_write	__db_jump.j_write	/* __db_write is a wrapper. */
#define	__db_yield	__db_jump.j_yield
