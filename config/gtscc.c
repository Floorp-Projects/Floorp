/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
/* */
/*
 *--------------------------------------------------------------------------
 *
 *    
 *
 *--------------------------------------------------------------------------
 *
 *    gtscc - Global To Static C/C++ compiler driver.
 *
 *    Syntax:
 *
 *    gtscc [options] -c file.cpp ...
 *    gtscc [options] file.o ... libxx.a ...
 *
 *    gtscc is a compiler and linker driver/wrapper for Irix only.
 *    gtscc takes all compiler options and passes them onto the Irix
 *    cc/CC compiler/linker.
 *    Typically, gtscc is used in two phases. Phase one is during compilation.
 *    gtscc, the compiler, converts all inline globals to statics, and records
 *    the existance of other globals and how to compile the file in the gtscc
 *    database file.
 *    During linking, globals dependencies are analyzed, and a list of
 *    "convertable" globals is determined. Globals that are not referenced
 *    globally, but are referenced locally are considered convertable.
 *    The linker then recompiles the files that those symbols are in, and
 *    converts them to statics. It also calls the archiver to install
 *    the converted objects into libraries.
 *    Finally the linker is called.
 *
 *    Created: David Williams, djw@netscape.com, 13-Feb-1997
 *
 *--------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#if defined(LINUX) && defined(__GLIBC__)
#include <libelf/libelf.h>
#else
#include <libelf.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#define DEFAULT_MAX_GLOBALS 15500

#define ELFSYM_IS_DEFINED(x)   ((x).st_shndx != SHN_UNDEF)
#define ELFSYM_IS_UNDEFINED(x) ((x).st_shndx == SHN_UNDEF)

#ifdef IRIX
#define CC_COMMAND  "cc"
#define CCC_COMMAND "CC"
#define AS_COMMAND  "cc"
#define LD_COMMAND  "CC"
#define AR_COMMAND  "ar"
#define AR_OPTIONS  "cr"
#else
#define HANDLES_DASHSO
#define CC_COMMAND  "gcc"
#define CCC_COMMAND "g++"
#define AS_COMMAND  "gcc"
#define LD_COMMAND  "g++"
#define AR_COMMAND  "ar"
#define AR_OPTIONS  "cr"
#endif

#define EH_NEW(type) (type*)malloc(sizeof(type))

#define TRUE 1
#define FALSE 0

#define EH_TAG_FILE   'F'
#define EH_TAG_GLOBAL 'G'
#define EH_TAG_ZAPPED 'Z'
#define EH_TAG_INLINE 'I'
#define EH_TAG_UNDEFINED 'U'

#define VERBOSITY_USER(x)  ((x) > 0)
#define VERBOSITY_DEBUG(x) ((x) > 1)
#define VERBOSITY_MAJOR(x) ((x) > 2)

static char eh_unnamed_object[] = "<name not known>";

typedef struct {
	char*    name;       /* archive */
} EhArchive;

typedef struct {
	char*      name;     /* name of C/C++ file, relative to rootdir */
	char*      directory;/* must compile in this directory */
	char**     cc_args;  /* cc -I ..... */
	char*      as_savefile;
	time_t     compile_time;
	char*      target_object;
} EhSource;

typedef struct EhObject {
	struct EhObject* _recompile; /* used for recompilation link list */
	unsigned   _needs_unzap;
	char*      name;     /* name of .o */
	EhArchive* archive;  /* it is stored in */
	EhSource*  source;
	char*      pathname;
	unsigned   nusers;
} EhObject;

typedef enum {
	EH_SYM_UNDEFINED,
	EH_SYM_DEFINED,
	EH_SYM_ZAPPED,
	EH_SYM_INLINE /* are treated special - they belong to no file */
} EhSymState;

typedef struct EhSym {
	struct EhSym* _next; /* used for link list */
	char*      name;     /* name of symbol */
	EhObject*  object;   /* if symbol is undefined == NULL */
	unsigned   ngusers;   /* number of global users */
	unsigned   nlusers;   /* number of local file users */

#if 0
	unsigned   section;  /* section in elf file */
	unsigned   index;    /* index into symbol table */
	unsigned char info;
	unsigned   dirty;
#endif
	EhSymState state;
} EhSym;

#define EHSYM_ISDEFINED(x)   ((x)->object!=NULL && (x)->state==EH_SYM_DEFINED)
#define EHSYM_ISZAPPED(x)    ((x)->object!=NULL && (x)->state==EH_SYM_ZAPPED)
#define EHSYM_ISUNDEFINED(x) ((x)->object == NULL)
#define EHSYM_ISUSED(x)      ((x)->nusers != 0)
#define EHSYM_ISINLINE(x)    ((x)->state == EH_SYM_INLINE)

#define EH_OBJECT_CANBUILD(x) \
((x)->source != NULL && (x)->name != eh_unnamed_object)

#define USE_HASHING

typedef struct {
#ifdef USE_HASHING
	EhSym** heads;
	unsigned size;
#else
	EhSym* head;
#endif
	unsigned nentries;
} EhSymTable;

static char*
make_relative_pathname(char* buf, char* filename, char* rootdir)
{
	char  buf1[MAXPATHLEN];
	char  buf2[MAXPATHLEN];
	char* p;
	char* q;

	if (rootdir == NULL) {
		strcpy(buf, filename);
		return filename;
	}

	if (filename[0] != '/') {
		if (getcwd(buf2, sizeof(buf2)) == NULL) {
			fprintf(stderr, "cannot get pwd\n");
			return NULL;
		}

		strcat(buf2, "/");
		strcat(buf2, filename);

		filename = buf2;
	}

	if (realpath(filename, buf1) == NULL) {
		fprintf(stderr, "realpath(%s,..) failed\n", filename);
		return NULL;
	}
	
	if (realpath(rootdir, buf2) == NULL) {
		fprintf(stderr, "realpath(%s,..) failed\n", rootdir);
		return NULL;
	}

	strcat(buf2, "/");

	for (p = buf1, q = buf2; *p == *q; p++, q++)
		;

	strcpy(buf, p);

	return buf;
}

static EhArchive*
EhArchiveNew(char* name, char* rootdir)
{
	EhArchive* archive = EH_NEW(EhArchive);
	char pathbuf[MAXPATHLEN];

	make_relative_pathname(pathbuf, name, rootdir);

	archive->name = strdup(pathbuf);

	return archive;
}

#if 0
/*
 *    This is evil, we should never free anything, because it messes up
 *    interning.
 */
static void
EhSourceDelete(EhSource* source)
{
	unsigned n;
	if (source->name != NULL)
		free(source->name);
	if (source->directory != NULL)
		free(source->directory);
	if (source->cc_args != NULL) {
		for (n = 0; source->cc_args[n] != NULL; n++)
			free(source->cc_args[n]);
		free(source->cc_args);
	}
	if (source->as_savefile != NULL)
		free(source->as_savefile);
}
#endif

static EhSource*
EhSourceNew(char* name, char** cc_args, char* directory)
{
	EhSource* source = EH_NEW(EhSource);
	unsigned n;
	unsigned m;

	source->name = strdup(name);
	source->directory = (directory != NULL)? strdup(directory): NULL;
	source->as_savefile = NULL;
	source->compile_time = 0;
	source->target_object = NULL;
	source->cc_args = NULL;

	if (cc_args != NULL) {

		for (n = 0; cc_args[n] != NULL; n++)
			;

		source->cc_args = (char**)malloc(sizeof(char*) * (n+1));

		for (m = 0, n = 0; cc_args[n] != NULL;) {
			if (strcmp(cc_args[n], "-o") == 0 && cc_args[n+1] != NULL) {
				source->target_object = strdup(cc_args[n+1]);
				n += 2;
			} else {
				source->cc_args[m++] =  strdup(cc_args[n++]);
			}
		}

		source->cc_args[m] = NULL;
	}

	return source;
}

static EhObject*
EhObjectNewArchiveObject(EhArchive* archive, char* name)
{
	EhObject* object = EH_NEW(EhObject);

	if (name == eh_unnamed_object)
		object->name = name;
	else
		object->name = strdup(name);
	object->archive = archive;
	object->source = NULL;
	object->_recompile = NULL;
	object->_needs_unzap = 0;
	object->pathname = NULL;
	object->nusers = 0;

	return object;
}

static EhObject*
EhObjectNew(char* name, char* rootdir)
{
	EhObject* object = EhObjectNewArchiveObject(NULL, name);
	char pathname[MAXPATHLEN];

	make_relative_pathname(pathname, name, rootdir);
	object->pathname = strdup(pathname);

	return object;
}

static EhObject*
EhObjectNewFromSource(EhSource* source)
{
	EhObject* object = EhObjectNewArchiveObject(NULL, eh_unnamed_object);

	object->source = source;

	return object;
}

static char*
EhObjectGetFilename(EhObject* object, char* buf)
{
	if (object->archive) {
		strcpy(buf, object->archive->name);
		strcat(buf, ":");
		strcat(buf, object->name);
		return buf;
	} else {
		return object->name;
	}
}

static EhSym*
EhSymNewDefined(char* name, EhObject* object)
{
	EhSym* sym = EH_NEW(EhSym);

	sym->name = strdup(name);
	sym->object = object;
	sym->state = EH_SYM_DEFINED;
	sym->ngusers = 0;
	sym->nlusers = 0;

	return sym;
}

static EhSym*
EhSymNewInline(char* name)
{
	EhSym* sym = EhSymNewDefined(name, NULL);
	sym->state = EH_SYM_INLINE;

	return sym;
}

static EhSym*
EhSymNewUndefined(char* name)
{
	EhSym* sym = EhSymNewDefined(name, NULL);
	sym->state = EH_SYM_UNDEFINED;

	return sym;
}

static EhSym*
EhSymNewZapped(char* name, EhObject* object)
{
	EhSym* sym = EhSymNewDefined(name, object);
	sym->state = EH_SYM_ZAPPED;

	return sym;
}

static EhSym*
EhSymNewRandomZap(char* name)
{
	EhSym* sym = EhSymNewZapped(name, NULL);

	return sym;
}

EhSymTable*
EhSymTableNew(unsigned p_size)
{
	EhSymTable* table = EH_NEW(EhSymTable);

#ifdef USE_HASHING
	unsigned size;
	for (size = 0x1; size < (16*1024); size <<= 1) {
		if (size >= p_size)
			break;
	}
	table->size = size;
	table->heads = (EhSym**)calloc(size, sizeof(EhSym*));
#else
	table->head = NULL;
#endif
	table->nentries = 0;

	return table;
}

EhSym*
EhSymTableInsert(EhSymTable* table, EhSym* sym)
{
#ifdef USE_HASHING
	unsigned long hash = elf_hash(sym->name);
	unsigned long mask = table->size - 1;
	unsigned index = (hash & mask);

	sym->_next = table->heads[index];
	table->heads[index] = sym;
#else
	sym->_next = table->head;
	table->head = sym;
#endif
	table->nentries++;

	return sym;
}

EhSym*
EhSymTableFind(EhSymTable* table, char* name)
{
	EhSym* sym;
	EhSym* head;

#ifdef USE_HASHING
	unsigned long hash = elf_hash(name);
	unsigned long mask = table->size - 1;
	unsigned index = (hash & mask);
	head = table->heads[index];
#else
	head = table->head;
#endif

	for (sym = head; sym != NULL; sym = sym->_next) {
		if (strcmp(name, sym->name) == 0)
			break;
	}

	return sym;
}

typedef int (*eh_dump_mappee_t)(EhSym* sym, void* arg);

static int
EhSymTableMap(EhSymTable* table, eh_dump_mappee_t func, void* arg)
{
	EhSym* sym;
	EhSym* head;

#ifdef USE_HASHING
	unsigned n;
	for (n = 0; n < table->size; n++) {
		head = table->heads[n];
#else
		head = table->head; {
#endif
		for (sym = head; sym != NULL; sym = sym->_next) {
			if ((func)(sym, arg) == -1)
				return -1;
		}
	}

	return 0;
}

typedef struct {
	EhObject* o_old;
	EhObject* o_new;
} fixup_info;

static int
fixup_mappee(EhSym* sym, void* arg)
{
	fixup_info* info = (fixup_info*)arg;

	if (sym->object == info->o_old)
		sym->object = info->o_new;

	return 0;
}

static EhObject*
EhSymTableObjectFixup(EhSymTable* table, EhObject* o_old, EhObject* o_new)
{
	fixup_info info;

	/*
	 *    Now visit every sym that pointed to tmp, and point it
	 *    at object.
	 */
	info.o_old = o_old;
	info.o_new = o_new;
	EhSymTableMap(table, fixup_mappee, &info);
	
	return o_new;
}



static char*
safe_fgets(char* buf, unsigned size, FILE* fp)
{
	unsigned nread = 0;

	if (buf == NULL)
		buf = (char*)malloc(size);

	for (;;) {

		if (fgets(&buf[nread], size - nread, fp) == NULL) {
			free(buf);
			return NULL;
		}

		if (strchr(buf, '\n') != NULL)
			return buf;

		/*
		 *    fgets returns n-1 characters and \0
		 */
		nread += (size - nread) - 1;
		size += 1024;
		buf = (char*)realloc(buf, size);
	}
}

static int
EhSymTableSetSymbolState(EhSymTable* table, char* name, EhSymState new_state)
{
	EhSym* sym = EhSymTableFind(table, name);

	if (sym == NULL) {
		sym = EhSymNewDefined(name, NULL);

		EhSymTableInsert(table, sym);
	}

	/* new_state must be EH_SYM_DEFINED || EH_SYM_ZAPPED */
	if (sym->state == EH_SYM_DEFINED || sym->state == EH_SYM_ZAPPED) {
		sym->state = new_state;
	} else if (sym->state == EH_SYM_INLINE) {
		char* state_name;
		if (new_state == EH_SYM_DEFINED)
			state_name = "global";
		else
			state_name = "static";
		fprintf(stderr,
				"WARNING: Symbol %s is an inline.\n"
				"         Forcing the symbol %s will be ignored.\n",
				name,
				state_name);
	} else { /* EH_SYM_UNDEFINED */
		/*
		 *    This call is being made after objects have started being
		 *    read. This is too late. I'm not sure I care though.
		 */
		return -1;
	}

	return 0;
}

static int
EhSymTableFpLoad(EhSymTable* table, FILE* fp)
{
	char* buf = NULL; /* I hope this is big enough */
	char* p;
	char* name;
	char* state;
	EhSym* sym;
	EhSource* source = NULL;
	EhObject* object = NULL;
	char* cc_args[512];
	unsigned n;
	unsigned line_n = 0;
	char* ctime = NULL;
	char* directory;
	char* savefile;

	while ((buf = safe_fgets(buf, 1024, fp)) != NULL) {

		if ((p = strchr(buf, '\n')) == NULL) {
			fprintf(stderr, "line to long: %d\n", line_n);
			return -1;
		}
		*p = '\0';

		line_n++;

		if (buf[0] == '!') /* comment */
			continue;

		for (p = buf; isspace(*p); p++)
			;

		name = p;
		for (; !isspace(*p); p++)
			;
		*p++ = '\0';

		if (name[0] == '\0')
			continue;
		
		for (; isspace(*p); p++)
			;

		state = p;
		for (; !isspace(*p) && *p != '\0'; p++)
			;
		*p++ = '\0';

		if (state[0] == EH_TAG_GLOBAL
			||
			state[0] == EH_TAG_ZAPPED
			||
			state[0] == EH_TAG_INLINE) {
			sym = EhSymTableFind(table, name);
			if (sym == NULL) { /* install a new one */
				
				if (source == NULL && state[0] != EH_TAG_INLINE) {
					fprintf(stderr,
							"[%d] found new style symbol (%s) but no source\n",
							line_n, name);
				}

				if (state[0] == EH_TAG_GLOBAL)
					sym = EhSymNewDefined(name, object);
				else if (state[0] == EH_TAG_INLINE)
					sym = EhSymNewInline(name);
				else
					sym = EhSymNewZapped(name, object);
				
				EhSymTableInsert(table, sym);
			} else {
				if (state[0] == EH_TAG_GLOBAL) {
					if (sym->state != EH_SYM_DEFINED) {
						fprintf(stderr,
								"out of sync defined symbol: %s, fixing\n",
								sym->name);
						sym->state = EH_SYM_DEFINED;
					}
				} else if (state[0] == EH_TAG_INLINE) {
					if (sym->state != EH_SYM_INLINE) {
						fprintf(stderr,
								"out of sync inlined symbol: %s, fixing\n",
								sym->name);
						sym->state = EH_SYM_INLINE;
					}
				} else {
					if (sym->state != EH_SYM_ZAPPED) {
						fprintf(stderr,
								"out of sync zapped symbol: %s, fixing\n",
								sym->name);
						sym->state = EH_SYM_ZAPPED;
					}
				}

#if 0
				/* these are probably "special" symbols like .div */
				if (sym->object != object) {
					fprintf(stderr,
							"out of sync object for symbol: %s, ignoring\n",
							sym->name);
				}
#endif
			}

			continue; /* no more fields we care about */
		} else if (state[0] == EH_TAG_FILE) {

			directory = p;
			for (; !isspace(*p) && *p != '\0'; p++)
				;
			*p++ = '\0';

			savefile = p;
			for (; !isspace(*p) && *p != '\0'; p++)
				;
			*p++ = '\0';

			ctime = p;
			for (; !isspace(*p) && *p != '\0'; p++)
				;
			*p++ = '\0';

			for (n = 0; *p != '\0';) {

				for (; isspace(*p); p++)
					;
			
				cc_args[n++] = p++;
				
				for (; !isspace(*p) && *p != '\0'; p++)
					;
				
				if (*p == '\0')
					break;

				*p++ = '\0';
			}
			cc_args[n] = NULL;

			if (strcmp(directory, ".") == 0)
				directory = NULL;
			source = EhSourceNew(name, cc_args, directory);
			if (ctime != NULL)
				source->compile_time = (time_t)atoi(ctime);
			object = EhObjectNewFromSource(source);

		} else { /* old style symbol list */
			sym = EhSymTableFind(table, name);
			if (sym != NULL) {
				if (sym->state != EH_SYM_ZAPPED) {
					fprintf(stderr,
							"out of sync random zapped symbol: %s, fixing\n",
							sym->name);
					sym->state = EH_SYM_ZAPPED;
				}
			} else {
				sym = EhSymNewRandomZap(name);
			}
		}
	}

	return line_n;
}

typedef struct {
	EhSym**  vector;
	unsigned index;
} flush_info;

static int
flush_mappee(EhSym* sym, void* arg)
{
	flush_info* info = (flush_info*)arg;

	if (sym->state == EH_SYM_INLINE
		||
		(sym->object != NULL && sym->state == EH_SYM_DEFINED)
		||
		(sym->object != NULL && sym->state == EH_SYM_ZAPPED)) {
		if (info->vector != NULL)
			info->vector[info->index] = sym;
		info->index++;
	}

	return 0;
}

static  int
flush_compare(const void* ap, const void* bp)
{
	EhSym** ax = (EhSym**)ap;
	EhSym** bx = (EhSym**)bp;
	EhSym* a = *ax;
	EhSym* b = *bx;
	EhObject* oa = a->object;
	EhObject* ob = b->object;
	int foo;

	if (oa == NULL && ob != NULL)
		return -1;
	if (oa != NULL && ob == NULL)
		return 1;
	if (oa == NULL && ob == NULL) {
		foo = strcmp(a->name, b->name);
		if (foo < 0)
			return -1;
		else if (foo > 0)
			return 1;
		return 0;
	}

	if (oa->source == NULL && ob->source != NULL)
		return -1;
	if (oa->source != NULL && ob->source == NULL)
		return 1;
	if (oa->source == ob->source)
		return 0;
	if (oa->source < ob->source)
		return -1;
	if (oa->source > ob->source)
		return 1;
	foo = strcmp(a->name, b->name);
	if (foo < 0)
		return -1;
	else if (foo > 0)
		return 1;
	return 0;
}

static void
EhSourceFpWrite(EhSource* source, FILE* fp)
{
	unsigned n = 0;

	fputs(source->name, fp);
	fputc(' ', fp);
	fputc(EH_TAG_FILE, fp);

	fputc(' ', fp);
	if (source->directory != NULL)
		fprintf(fp, "%s", source->directory);
	else
		fputc('.', fp);
	
	fputc(' ', fp);
	if (source->as_savefile != NULL)
		fprintf(fp, "%s", source->as_savefile);
	else
		fputc('.', fp);
	
	fputc(' ', fp);
	fprintf(fp, "%d", source->compile_time);

	if (source->target_object != NULL) {
		fputs(" -o ", fp);
		fputs(source->target_object, fp);
	}
	
	if (source->cc_args != NULL) {
		for (n = 0; source->cc_args[n] != NULL; n++) {
			fputc(' ', fp);
			fputs(source->cc_args[n], fp);
		}
	}

	if (n < 1)
		fprintf(stderr, "WARNING: %s has no args\n", source->name);

	fputc('\n', fp);
}

static int
EhSymTableFpDump(EhSymTable* table, FILE* fp)
{
	flush_info info;
	unsigned n;
	EhObject* object = NULL;
	EhSym**   syms;
	EhSym*    sym;
	unsigned size;

	info.index = 0;
	info.vector = NULL;
	EhSymTableMap(table, flush_mappee, (void*)&info);
	size = info.index;

	syms = (EhSym**)malloc(sizeof(EhSym*) * size);
	info.index = 0;
	info.vector = syms;
	EhSymTableMap(table, flush_mappee, (void*)&info);

	/* sort */
	qsort(syms, size, sizeof(EhSym*), flush_compare);

	/* dump */
	for (n = 0; n < size; n++) {
		sym = syms[n];

		if (sym->object != object) {
			object = sym->object;

			if (object->source != NULL) {
				EhSourceFpWrite(object->source, fp);
			}
		}

		if (sym->state == EH_SYM_INLINE) {
			fprintf(fp, "%s %c\n", sym->name, EH_TAG_INLINE);
		} else if (object->source != NULL && sym->state == EH_SYM_ZAPPED) {
			fprintf(fp, "%s %c\n", sym->name, EH_TAG_ZAPPED);
		} else if (object->source != NULL && sym->state == EH_SYM_DEFINED) {
			fprintf(fp, "%s %c\n", sym->name, EH_TAG_GLOBAL);
		}
	}

	free(syms);

	return n;
}

int djw_debug;
char* djw_test_name;

int
eh_process_object(Elf* elf, EhObject* object, EhSymTable* table)
{
	Elf32_Shdr *   shdr;
	Elf32_Ehdr *   ehdr;
	Elf_Scn * scn;
	Elf_Data *     shstr_data;
	Elf_Data*      sym_data = NULL;
	Elf_Data*      str_data = NULL;
	Elf_Data*      rel_data[4];
	int            nrel_data = 0;
	Elf32_Rel*     rel_entries;
	Elf_Data*      rela_data[10];
	int            nrela_data = 0;
	Elf32_Rela*    rela_entries;
	unsigned int   cnt;
	Elf32_Sym* elf_sym;
	int i;
	int j;
	int k;
	char*  name;
	EhSym* sym;
	char buf[MAXPATHLEN];

	/* Obtain the .shstrtab data buffer */
	if (((ehdr = elf32_getehdr(elf)) == NULL) ||
		((scn = elf_getscn(elf, ehdr->e_shstrndx)) == NULL) ||
		((shstr_data = elf_getdata(scn, NULL)) == NULL)) {
		fprintf(stderr, "problems on %s\n", EhObjectGetFilename(object, buf));
		return -1;
	}

	/* get the string table */
	for (cnt = 1, scn = NULL; (scn = elf_nextscn(elf, scn)); cnt++) {
		if ((shdr = elf32_getshdr(scn)) == NULL) {
			fprintf(stderr, "problems on %s, section %d\n",
					EhObjectGetFilename(object, buf), cnt);
			return -1;
		}

#if 0
		fprintf(stderr, "%s: section %d type %d name %s\n",
				EhObjectGetFilename(object, buf),
				cnt,
				shdr->sh_type,
				(char*)shstr_data->d_buf + shdr->sh_name);
#endif

		/*
		 *    Get the string table.
		 */
		if (shdr->sh_type == SHT_STRTAB &&
#ifdef sun
			strcmp((char*)shstr_data->d_buf + shdr->sh_name, ".strtab") == 0 &&
#endif
			cnt != ehdr->e_shstrndx) {
			if (str_data != NULL) {
				fprintf(stderr, "multiple string tables for %s - bailing\n",
						EhObjectGetFilename(object, buf));
				return -1;
			}
			str_data = elf_getdata(scn, NULL);
		} else if (shdr->sh_type == SHT_SYMTAB) { /* look into sym table */
			if (sym_data != NULL) {
				fprintf(stderr, "multiple symbol tables for %s - bailing\n",
						EhObjectGetFilename(object, buf));
				return -1;
			}
			sym_data = elf_getdata(scn, NULL);
		} else if (shdr->sh_type == SHT_REL) { /* look into rel table */
			if (nrel_data >= 4) {
				fprintf(stderr, "too many relocation tables for %s bailing\n",
						EhObjectGetFilename(object, buf));
				return -1;
			}
			rel_data[nrel_data++] = elf_getdata(scn, NULL);
		} else if (shdr->sh_type == SHT_RELA) { /* look into rela table */
			if (nrela_data >= 10) {
				fprintf(stderr, "too many RELA tables for %s bailing\n",
						EhObjectGetFilename(object, buf));
				return -1;
			}
			rela_data[nrela_data++] = elf_getdata(scn, NULL);
		}
	}

	if (sym_data == NULL) {
		fprintf(stderr, "could not load sym table for %s\n",
				EhObjectGetFilename(object, buf));
		return -1;
	}

	if (str_data == NULL) {
		fprintf(stderr, "could not load string table for %s\n",
				EhObjectGetFilename(object, buf));
		return -1;
	}

	elf_sym = (Elf32_Sym*)sym_data->d_buf;

	for (i = 0; i < (sym_data->d_size/sizeof(Elf32_Sym)); i++) {

		/*
		 *    We are only interested in globals.
		 */
		if (ELF32_ST_BIND(elf_sym[i].st_info) != STB_GLOBAL)
			continue;
		
		name = (char *)str_data->d_buf + elf_sym[i].st_name;
		
		if (djw_test_name != NULL
			&& strcmp(djw_test_name, name) == 0) {
			printf("found %s\n", name);
		}
		
		sym = EhSymTableFind(table, name);
		
		/*
		 *    Treat inlines as non-globals
		 */
		if (sym != NULL && sym->state == EH_SYM_INLINE)
			continue;
		
#if 0
		printf("name = %s value = %d type = %d, info = %d,"
			   " other = %d, size = %d\n",
			   name,
			   elf_sym[i].st_value,
			   ELF32_ST_TYPE(elf_sym[i].st_info),
			   elf_sym[i].st_info,
			   elf_sym[i].st_other,
			   elf_sym[i].st_size);
#endif
		
		/* defined */
		if (ELFSYM_IS_DEFINED(elf_sym[i])) {
			
			if (sym != NULL) {
				
				if (sym->object == NULL) { /* object undefined */
					sym->object = object;
				} else if (sym->object->name==eh_unnamed_object) {
					
					if (object->source != NULL
						&&
						object->source != sym->object->source) {
						
						fprintf(stderr,
								"warning: symbol %s defined in more than one source file\n"
								"last time: %s\n"
								"this time: %s (ignored)\n",
								sym->name,
								object->source->name,
								sym->object->source->name);
					} else {
						object->source = sym->object->source;
						/*
						 *    Do a global: sym->object = object;
						 */
						EhSymTableObjectFixup(table,
											  sym->object, /*old*/
											  object); /*new*/
						
					}
					
				} else if (sym->object != object) {
					fprintf(stderr,
							"warning: symbol %s define in multiple object files\n"
							"last time: %s\n"
							"this time: %s (ignored)\n",
							sym->name,
							object->name,
							sym->object->name);
				}
				
				sym->state = EH_SYM_DEFINED;

			} else {
				sym = EhSymNewDefined(name, object);
				EhSymTableInsert(table, sym);
			}				

	        for (k = 0; k < nrel_data; k++) {
				int nentries = rel_data[k]->d_size/sizeof(Elf32_Rel);

				rel_entries = (Elf32_Rel*)rel_data[k]->d_buf;
				
				for (j = 0; j < nentries; j++) {
					if (ELF32_R_SYM(rel_entries[j].r_info) == i) {
						/* locally referenced */
						sym->nlusers++;
					}
				}
			}
	        for (k = 0; k < nrela_data; k++) {
				int nentries = rela_data[k]->d_size/sizeof(Elf32_Rela);

				rela_entries = (Elf32_Rela*)rela_data[k]->d_buf;
				
				for (j = 0; j < nentries; j++) {
					if (ELF32_R_SYM(rela_entries[j].r_info) == i) {
						/* locally referenced */
						sym->nlusers++;
					}
				}
			}
		}  
		
		/* Undefined. */
		else if (ELFSYM_IS_UNDEFINED(elf_sym[i])) {
			
			if (sym == NULL) {
				sym = EhSymNewUndefined(name);
				EhSymTableInsert(table, sym);
			}
			sym->ngusers++;
		} else {
			
#if 1
			printf("what is this: "
				   "name = %s value = %d type = %d, "
				   "info = %d, other = %d, size = %d\n",
				   name,
				   elf_sym[i].st_value,
				   ELF32_ST_TYPE(elf_sym[i].st_info),
				   elf_sym[i].st_info,
				   elf_sym[i].st_other,
				   elf_sym[i].st_size);
#endif
			;
		}/* type ==... */
	} /* for each symbol */

	return 0;
}

int
eh_process_file(char* filename, EhSymTable* table, char* rootdir)
{
	Elf* elf;
	Elf* arf;
	int  fd;
	Elf_Cmd cmd;
	Elf_Kind e_kind;
	EhObject* object;
	EhArchive* archive;
	Elf_Arhdr* arhdr;
	char* name;
	int   rv = 0;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		fprintf(stderr, "error opening %s\n", filename);
		return -1;
	}

	elf_version(EV_CURRENT);
	if ((arf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		return -1;
	}

	e_kind = elf_kind(arf);
	if (e_kind == ELF_K_ELF) {
		object = EhObjectNew(filename, rootdir);
		rv = eh_process_object(arf, object, table);
		
	} else if (e_kind == ELF_K_AR) {

		archive = EhArchiveNew(filename, rootdir);
		cmd = ELF_C_READ;

#if 0
		arsyms = elf_getarsym(arf, &narsyms);

		for (i = 0; i < narsyms && arsyms[i].as_name != NULL; i++) {
			printf("%s - %d\n", arsyms[i].as_name, arsyms[i].as_off);
		}

		arhdr = elf_getarhdr(arf);
		for (i = 0; arhdr[i].ar_rawname != NULL; i++) {

			if (arhdr[i].ar_name != NULL)
				printf("%s\n", arhdr[i].ar_name);
			else
				printf("[%s]\n", arhdr[i].ar_rawname);
		}
#endif

		rv = 0;

		while ((elf = elf_begin(fd, cmd, arf)) != 0) {

			e_kind = elf_kind(elf);

			if (e_kind != ELF_K_ELF)
				continue;

			arhdr = elf_getarhdr(elf);

			if (arhdr != NULL) {
				if (arhdr->ar_name != NULL)
					name = arhdr->ar_name;
				else
					name = arhdr->ar_rawname;
			} else {
				name = eh_unnamed_object;
			}

			object = EhObjectNewArchiveObject(archive, name);
			rv = eh_process_object(elf, object, table);

			if (rv == -1)
				break;

			cmd = elf_next(elf);
			elf_end(elf);
		}
	}

	elf_end(arf);

	close(fd);

	return rv;
}

static int
eh_dump_unused(EhSym* sym, void* arg)
{
	char buf[MAXPATHLEN];

	printf(/*"0x%x "*/ "%s %d %d ", /*sym,*/
		   sym->name, sym->ngusers, sym->nlusers);

	if (EHSYM_ISINLINE(sym))
		printf("%c ", EH_TAG_INLINE);
	else if (EHSYM_ISZAPPED(sym))
		printf("%c ", EH_TAG_ZAPPED);
	else if (EHSYM_ISDEFINED(sym))
		printf("%c ", EH_TAG_GLOBAL);
	else
		printf("%c ", EH_TAG_UNDEFINED);

	if (sym->object != NULL) {
		printf("%s ", EhObjectGetFilename(sym->object, buf));
		if (sym->object->source != NULL) {
			printf("%s recompilable\n", sym->object->source->name);
		} else {
			printf("nosource notrecompilable\n");
		}
	} else {
		printf("noobject nosource notrecompilable\n");
	}
	
	return 0;
}

static void
print_dump(EhSymTable* table)
{
	printf("everything\n");
	EhSymTableMap(table, eh_dump_unused, NULL);
}

typedef struct {
	unsigned ndefined;
	unsigned nused; /* globally */
	unsigned nundefined;
	unsigned nzapped;
	unsigned nzapped_nowused;
	unsigned ninlined;
	unsigned nunlinked;
	unsigned ndeadcode;
} SummaryInfo;

static int
eh_summary_mappee(EhSym* sym, void* arg) {
	SummaryInfo* info = (SummaryInfo*)arg;

	if (EHSYM_ISDEFINED(sym)) {
		if (sym->ngusers != 0)
			info->nused++;
		else if (sym->object != NULL && sym->object->nusers == 0)
			info->nunlinked++;
		else if (sym->nlusers != 0)
			info->ndefined++;
		else
			info->ndeadcode++;
			
	} else if (EHSYM_ISZAPPED(sym)) { /* one of ours */
		if (sym->ngusers != 0)
			info->nzapped_nowused++;
		else
			info->nzapped++;
	} else if (EHSYM_ISINLINE(sym)) { /* one of ours */
		info->ninlined++;
	} else {
		info->nundefined++;
	}

	return 0;
}

static void
get_summary(EhSymTable* table, SummaryInfo* info)
{
	info->ndefined = 0;
	info->nused = 0;
	info->nundefined = 0;
	info->nzapped = 0;
	info->nzapped_nowused = 0;
	info->ninlined = 0;
	info->nunlinked = 0;
	info->ndeadcode = 0;
	
	EhSymTableMap(table, eh_summary_mappee, info);
} 

static void
print_summary(EhSymTable* table)
{
	SummaryInfo info;

	get_summary(table, &info);
	
	printf("summary:\n"
		   "defined and used:             %d\n"
		   "defined but unused globally:  %d\n"
		   "total globals in target:      %d\n"
		   "--------------------------------\n"
		   "global to statics *:          %d\n"
		   "global to statics (now used): %d\n"
		   "inlined to statics *:         %d\n"
		   "defined in unlinked objects:  %d\n"
		   "defined but unused (deadcode):%d\n"
		   "undefined but used:           %d\n",
		   info.nused,
		   info.ndefined,
		   (info.nused + info.ndefined),
		   info.nzapped,
		   info.nzapped_nowused,
		   info.ninlined,
		   info.nunlinked,			   
		   info.ndeadcode,			   
		   info.nundefined);
}

typedef struct EhDirMapEntree {
	char* dirname;
	struct EhDirMapEntree* _next;
} EhDirMapEntree;

typedef struct EhDirMap {
	EhDirMapEntree* head;
} EhDirMap;

static EhDirMap*
EhDirMapNew(void)
{
	EhDirMap* dm = EH_NEW(EhDirMap);
	dm->head = NULL;
	return dm;
}

static void
EhDirMapAddDirectory(EhDirMap* map, char* dirname)
{
	EhDirMapEntree* entree = EH_NEW(EhDirMapEntree);
	EhDirMapEntree* foo;

	entree->dirname = strdup(dirname);
	entree->_next = NULL;

	if (map->head == NULL) {
		map->head = entree;
	} else {
		for (foo = map->head; foo->_next != NULL; foo = foo->_next)
			;

		foo->_next = entree;
	}
}

static char*
EhDirMapGetLibName(EhDirMap* map, char* name, char* libbuf)
{
	EhDirMapEntree* foo;
	struct stat     buf;

	for (foo = map->head; foo != NULL; foo = foo->_next) {
		sprintf(libbuf, "%s/lib%s.a", foo->dirname, name);

		if (stat(libbuf, &buf) != -1)
			return libbuf;
	}

	return NULL;
}

static char*
test_for_global(char* buf)
{
#ifdef IRIX
	if (strncmp(buf, "\t.globl\t", 8) == 0)
		return &buf[8];
#else
	if (strncmp(buf, "\t.global ", 9) == 0)
		return &buf[9];
#endif
	return NULL;
}

static char*
test_for_file(char* foo, char* buf)
{
#ifdef IRIX
	char* p;
	char* q;
	if (strncmp(buf, "\t.file\t", 6) == 0) {
		for (p = &buf[6]; *p != '"' && *p != '\0'; p++)
			;
		if (*p == '\0')
			return NULL;
		p++; /* quote */
		q = strchr(p, '"');
		if (q == NULL)
			return NULL;
		memcpy(foo, p, q - p);
		foo[q - p] = '\0';
		return foo;
	}
#else
	printf("test_for_file() not implimented\n");
#endif
	return NULL;
}

static int
EhSourceZapFp(EhSource* source, EhSymTable* table, FILE* fpi, FILE* fpo,
			  unsigned verbosity, unsigned cplusplus)
{
	char*     buf = NULL;
	char*     p;
	int       nzap = 0;
	char*     name;
	EhSym*    sym;
	int       i_zapped;
	EhObject* object = EhObjectNewFromSource(source);
	char*     filename = source->name;
	char*     tmp_name;
	char      foo[256];
	unsigned  line_n = 0;

	if (VERBOSITY_DEBUG(verbosity))
		fputs("gts: ", stderr);

	while ((buf = safe_fgets(buf, 4192, fpi)) != NULL) {

		i_zapped = 0;
		for (p = buf; *p != '\0' && *p != '\n'; p++)
			;
		*p = '\0';

		if ((tmp_name = test_for_file(foo, buf)) != NULL) {
			if (strcmp(tmp_name, filename) != 0) /* not the same file */
				filename = "non local file";
			else
				filename = source->name;
		}

		else if ((name = test_for_global(buf)) != NULL) {

			sym = EhSymTableFind(table, name);

			/* an inline, we have to treat specially */
			if ((filename != source->name && cplusplus != 0) /* inline now */
				||
				(sym != NULL && sym->state == EH_SYM_INLINE)) {/* was inline */

				if (!sym) { 

					sym = EhSymNewInline(name);
					
					EhSymTableInsert(table, sym);
				}
				sym->state = EH_SYM_INLINE; /* just make sure */

				if (fpo != NULL) /* always zap inlines we see */
					fputs(" # gts", fpo);

			} else { /* a real global */

				if (fpo != NULL && sym != NULL && EHSYM_ISZAPPED(sym)) {
					if (VERBOSITY_DEBUG(verbosity)) {
						fprintf(stderr, "%s ", &buf[8]);
					}
					nzap++;

					if (fpo != NULL)
						fputs(" # gts", fpo);
				}

				if (sym != NULL) {
					if (sym->object == NULL) {
						sym->object = object;
					} else if (sym->object != object) {
						sym->object->source = source;
						EhSymTableObjectFixup(table, object, sym->object);
						object = sym->object;
					}
				} else { /* install a new one */
					
					sym = EhSymNewDefined(name, object);
					
					EhSymTableInsert(table, sym);
				}

			}
		}

		if (fpo != NULL) {
			fputs(buf, fpo);
			fputc('\n', fpo);
		}
		line_n++;
	}
	
	if (VERBOSITY_DEBUG(verbosity))
		fputc('\n', stderr);

	return nzap;
}

static void
print_command(char* command, char** args, unsigned verbosity)
{
	int i;
	FILE* fp = stderr;
	if (!VERBOSITY_USER(verbosity))
		return;

	fprintf(fp, "%s: ", command);
	for (i = 0; args[i]; i++) {
		fprintf(fp, "%s ", args[i]);
	}
	fprintf(fp, "\n");
}

static int
do_command(char* label, char** args, unsigned verbosity)
{
	int status;
	pid_t child_pid;
	char* file = args[0];

	print_command(label, args, verbosity);

	if ((child_pid = fork()) == -1) {
		fprintf(stderr, "could not fork: ");
		perror(NULL);
		return -1;
	}

	if (child_pid == 0) { /* i am the child */
		if (execvp(file, args) == -1) {
			fprintf(stderr, "could not exec %s: ", file);
			perror(NULL);
			exit(3);
		}
		/*NOTREACHED*/
	}

	if (waitpid(child_pid, &status, 0) == -1) {
		fprintf(stderr, "wait on %s failed: ", file);
		perror(NULL);
		return -1;
	}

	return WEXITSTATUS(status);
}

static char*
suffix_name(char* s)
{
	char* p;

	if ((p = strrchr(s, '.')) != NULL)
		return p;
	else
		return "";
}

static char base_name_buf[MAXPATHLEN];

static char*
base_name(char* s)
{
	char* p;

	if ((p = strrchr(s, '.')) != NULL) {
		memcpy(base_name_buf, s, p - s);
		base_name_buf[p - s] = '\0';
		s = base_name_buf;
	}
	return s;
}

static char*
file_base_name(char *s)
{
	char* p;

	s = base_name(s);

	if ((p = strrchr(s, '/')) != NULL)
		s = &p[1];
	
	return s;
}
static int
EhSourceCompile(EhSource*   source,
				EhSymTable* table,
				unsigned    verbosity,
				unsigned    do_compile,
				unsigned    do_zap,
				unsigned    do_assem)
{
	char* filename = source->name;
	char** opts = source->cc_args;
	char asname[MAXPATHLEN];
	char o_asname[MAXPATHLEN];
	char* cc_opts[256];
	unsigned i = 0;
	unsigned j = 0;
	FILE* in_fp;
	FILE* out_fp;
	int   status;
	int nzap = 0;
	char* save_prefix = NULL;
	unsigned do_dash_s = (do_zap != 0 || save_prefix != NULL);
	char* cc_command;
	char* use_savefile = NULL;
	struct timeval start_time;
	struct timeval end_time;
#ifdef DEBUG_djw
	char savebuf[1024];
#endif
	unsigned is_cplusplus = 0;

#ifdef LINUX
	gettimeofday(&start_time,NULL);
#else
	gettimeofday(&start_time);
#endif

	/* munge file */
#ifdef HANDLES_DASHSO
	if (source->target_object != NULL)
		strcpy(asname, base_name(source->target_object));
	else
#endif
		strcpy(asname, file_base_name(filename));
	strcat(asname, ".s");

	strcpy(o_asname, asname);
	strcat(o_asname, ".gts_tmp");

	if (strcmp(suffix_name(filename), ".cpp") == 0) {
		cc_command = CCC_COMMAND;
		is_cplusplus = 1;
	} else if (strcmp(suffix_name(filename), ".s") == 0) {
		do_compile = FALSE;
		cc_command = CC_COMMAND;
	} else {
		cc_command = CC_COMMAND;
	}

	if (do_compile) {

		j = 0;
		cc_opts[j++] = cc_command;
		cc_opts[j++] = "-c";

		if (do_dash_s) {
			cc_opts[j++] = "-S";
#ifdef HANDLES_DASHSO
			if (source->target_object != NULL) {
				cc_opts[j++] = "-o";
				cc_opts[j++] = asname;
			}
#endif
		} else if (source->target_object != NULL) {
			cc_opts[j++] = "-o";
			cc_opts[j++] = source->target_object;
		}
		
		i = 0;
		while (opts[i] != NULL)
			cc_opts[j++] = opts[i++];
		
		cc_opts[j++] = filename;
		cc_opts[j] = NULL;
		
		if ((status = do_command("compile", cc_opts, verbosity)) != 0) {
			fprintf(stderr, "compile failed (returned %d)\n", status);
			return -1;
		}

		if (!do_dash_s)
			return 0;
	}

	/*
	 *    Now we have a foo.s file, what do we do with it?
	 */
	if (do_zap > 1) {

		if (use_savefile == NULL)
			use_savefile = asname;

		if ((in_fp = fopen(use_savefile, "r")) == NULL) {
			fprintf(stderr, "could not open %s for reading\n", asname);
			return -1;
		}
	
		if ((out_fp = fopen(o_asname, "w")) == NULL) {
			fprintf(stderr, "could not open %s for writing\n", o_asname);
			return -1;
		}
	
		j = 0;
		cc_opts[j++] = "gts";
		cc_opts[j++] = asname;
		cc_opts[j++] = o_asname;
		cc_opts[j++] = NULL;
		print_command("gts", cc_opts, verbosity);

		nzap = EhSourceZapFp(source, table, in_fp, out_fp, verbosity, is_cplusplus);

		fclose(in_fp);
		fclose(out_fp);

		j = 0;
		cc_opts[j++] = "rename";
		cc_opts[j++] = o_asname;
		cc_opts[j++] = asname;
		cc_opts[j++] = NULL;
		print_command("rename", cc_opts, verbosity);

#ifdef DEBUG_djw
		strcpy(savebuf, "gts_pre_");
		strcat(savebuf, asname);
		rename(asname, savebuf);
#endif

		if (rename(o_asname, asname) == -1) {
			fprintf(stderr, "could not rename %s\n", o_asname);
			return -1;
		}

	} else if (do_zap > 0) { /* audit only */

		if ((in_fp = fopen(asname, "r")) == NULL) {
			fprintf(stderr, "could not open %s for reading\n", asname);
			return -1;
		}
	
		j = 0;
		cc_opts[j++] = "audit";
		cc_opts[j++] = asname;
		cc_opts[j++] = NULL;
		print_command("audit", cc_opts, verbosity);

		nzap = EhSourceZapFp(source, table, in_fp, NULL, verbosity, is_cplusplus);

		fclose(in_fp);
	}

	if (do_assem) {
		i = 0;
		j = 0;
		cc_opts[j++] = AS_COMMAND;
		cc_opts[j++] = "-c";

		if (source->target_object != NULL) {
			cc_opts[j++] = "-o";
			cc_opts[j++] = source->target_object;
		}

		while (opts[i] != NULL)
			cc_opts[j++] = opts[i++];

		cc_opts[j++] = asname;
		cc_opts[j] = NULL;

		if ((status = do_command("assemble", cc_opts, verbosity)) != 0) {

			unlink(asname);

			fprintf(stderr,
				"gtscc of %s failed (exit status = %d), reverting to %s:\n",
					filename,
					status,
					cc_command);
			
			i = 0;
			j = 0;
			cc_opts[j++] = cc_command;
			cc_opts[j++] = "-c";

			if (source->target_object != NULL) {
				cc_opts[j++] = "-o";
				cc_opts[j++] = source->target_object;
			}

			for (; opts[i]; i++, j++)
				cc_opts[j] = opts[i];
			
			cc_opts[j++] = filename;
			cc_opts[j] = NULL;
			
			if ((status = do_command("fix-compile", cc_opts, verbosity)) != 0)
				return -1;

			return 0;
		}
	}

	if (save_prefix != NULL && save_prefix[0] != '\0') {

		sprintf(o_asname, save_prefix, file_base_name(filename));

		j = 0;
		cc_opts[j++] = "rename";
		cc_opts[j++] = asname;
		cc_opts[j++] = o_asname;
		cc_opts[j++] = NULL;
		print_command("savefile", cc_opts, verbosity);

		if (rename(asname, o_asname) == -1) {
			fprintf(stderr, "could not rename %s to %s - sorry\n",
					asname, o_asname);
			return -1;
		}

		if (source->as_savefile != NULL)
			free(source->as_savefile);
		source->as_savefile = strdup(o_asname);
	} else {

		j = 0;
		cc_opts[j++] = "unlink";
		cc_opts[j++] = asname;
		cc_opts[j++] = NULL;
		print_command("unlink", cc_opts, verbosity);

#ifdef DEBUG_djw
		strcpy(savebuf, "gts_post_");
		strcat(savebuf, asname);
		rename(asname, savebuf);
#else
		unlink(asname);
#endif
	}

#ifdef LINUX
	gettimeofday(&end_time,NULL);
#else
	gettimeofday(&end_time);
#endif

	source->compile_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                           ((end_time.tv_usec - start_time.tv_usec) / 1000);

	return nzap;
}

static char target_buf[MAXPATHLEN]; /* this will go away with rel source */

static char*
EhSourceGetTarget(EhSource* source)
{
	if (source->target_object != NULL)
		return source->target_object;

	strcpy(target_buf, base_name(source->name));
	strcat(target_buf, ".o");

	return target_buf;
}

static int
EhArchiveUpdate(EhArchive* archive, char* target, char* rootdir,
				unsigned verbosity)
{
	char* args[8];
	unsigned nargs;
	int status;
	char pathname[MAXPATHLEN];

	pathname[0] = '\0';
	if (rootdir != NULL) {
		strcat(pathname, rootdir);
		strcat(pathname, "/");
	}
	strcat(pathname, archive->name);

#if 0
	nargs = 0;
	args[nargs++] = AR_COMMAND;
	args[nargs++] = "dc";
	args[nargs++] = pathname;
	args[nargs++] = target;
	args[nargs++] = NULL;
	
	if ((status = do_command("delete from archive", args, verbosity)) != 0) {
		fprintf(stderr, "archive: %s delete %s failed (status = %d)\n",
				pathname,
				target);
		return -1;
	}
#endif

	nargs = 0;
	args[nargs++] = AR_COMMAND;
	args[nargs++] = AR_OPTIONS;
	args[nargs++] = pathname;
	args[nargs++] = target;
	args[nargs++] = NULL;
	
	if ((status = do_command("archive", args, verbosity)) != 0) {
		fprintf(stderr, "archive: %s <- %s failed (status = %d)\n",
				pathname,
				target);
		return -1;
	}

	return 0;
}

static int
EhObjectRebuild(EhObject*   object,
				EhSymTable* table,
				unsigned    verbosity,
				char*       rootdir)
{
	EhSource* source = object->source;
	char  cwd[MAXPATHLEN];
	char  fullpath[MAXPATHLEN];
	int   rv = 0;
	int   do_chdir = 0;

	if (!source) {
		fprintf(stderr,
				"wanted to recompile %s, but I don't how\n",
				object->name);
		return -1;
	}
	
#if 0
	if (VERBOSITY_USER(verbosity))
#endif
		fprintf(stderr, "recompiling %s\n", source->name);
	
	/*
	 *    Check to see if we need to chdir
	 */
	if (source->directory != NULL) {
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "cannot get pwd: cannot compile\n");
			return -1;
		}
		
		make_relative_pathname(fullpath, cwd, rootdir);
		
		if (strcmp(fullpath, source->directory) != 0) {
			fullpath[0] = '\0';
			if (rootdir != NULL) {
				strcat(fullpath, rootdir);
				strcat(fullpath, "/");
			}
			strcat(fullpath, source->directory);

			if (chdir(fullpath) == -1) {
				fprintf(stderr, "cannot chdir - can't compile\n");
				return -1;
			}
			do_chdir++;
		}
	}

	rv = EhSourceCompile(source,
						 table,
						 verbosity,
						 TRUE,  /* compile  */
						 2,     /* do zap   */
						 TRUE); /* do assem */

	if (do_chdir) {
		if (chdir(cwd) == -1) {
			fprintf(stderr, "cannot chdir - this will be very confused\n");
			return -1;
		}
	}

	if (rv == -1) {
		fprintf(stderr, "recompiling %s failed\n",  source->name);
		return -1;
	}
	
	/* do archive */
	fullpath[0] = '\0';
	if (rootdir != NULL) {
		strcat(fullpath, rootdir);
		strcat(fullpath, "/");
	}

	if (source->directory != NULL)
		strcat(fullpath, source->directory);

	strcat(fullpath, "/");
	strcat(fullpath, EhSourceGetTarget(source));
	
	if (object->archive != NULL) {
		if (EhArchiveUpdate(object->archive, fullpath, rootdir,
							verbosity) == -1)
			return -1;
	}

	/* do install */
#if 0
	if (rv != -1) {
	}
#endif

	return rv;
}

static int
object_nusers_mappee(EhSym* sym, void* arg)
{
	if (sym->object != NULL)
		sym->object->nusers += sym->ngusers;
	return 0;
}

typedef struct {
	EhObject* recompile_list;
	unsigned  recompile_count;
	unsigned  recompile_wish_count;
	unsigned  unzap_count;
	unsigned  zap_count;
} RecompileInfo;

static int
recompile_init_mappee(EhSym* sym, void* arg)
{
	RecompileInfo* info = (RecompileInfo*)arg;

	if (EHSYM_ISZAPPED(sym) && sym->ngusers != 0) {
		if (EH_OBJECT_CANBUILD(sym->object)) {
			sym->state = EH_SYM_DEFINED;
			if (sym->object->_recompile == NULL) {
				sym->object->_recompile = info->recompile_list;
				info->recompile_list = sym->object;
				info->recompile_count++;
			}
			info->unzap_count++;
			sym->object->_needs_unzap++;
		}
		info->recompile_wish_count++;
	}
	else if (EHSYM_ISDEFINED(sym) /* it's defined */
			 && sym->ngusers == 0 /* there are no global users */
			 && sym->nlusers != 0 /* BUT, ther are local users */
			 && sym->object->nusers != 0) { /* object is linked */

		if (EH_OBJECT_CANBUILD(sym->object)) {
			sym->state = EH_SYM_ZAPPED;
			if (sym->object->_recompile == NULL) {
				sym->object->_recompile = info->recompile_list;
				info->recompile_list = sym->object;
				info->recompile_count++;
			}
			info->zap_count++;
		}
		info->recompile_wish_count++;
	}

	return 0;
}

static char**    recompile_compare_prefs;
static char**    recompile_compare_unprefs;

static unsigned
match_prefs(char* candidate, char** prefs)
{
	unsigned n;

	for (n = 0; prefs[n] != NULL; n++) {
		char*    pref = prefs[n];
		unsigned len = strlen(pref);
		if (strncmp(pref, candidate, len) == 0)
			return n; /* cool */
	}
	return (unsigned)-1; /* big! */
}

static  int
recompile_compare(const void* ap, const void* bp)
{
	EhObject** ax = (EhObject**)ap;
	EhObject** bx = (EhObject**)bp;
	EhObject*  obj_a = *ax;
	EhObject*  obj_b = *bx;
	EhSource*  src_a = obj_a->source;
	EhSource*  src_b = obj_b->source;
	unsigned   matcha;
	unsigned   matchb;
	int        foo;

	if (obj_a->_needs_unzap == 0 && obj_b->_needs_unzap != 0)
		return -1;
	if (obj_a->_needs_unzap != 0 && obj_b->_needs_unzap == 0)
		return 1;

	if (src_a == NULL && src_b != NULL)
		return 1;
	if (src_a != NULL && src_b == NULL)
		return -1;
	if (src_a == src_b)
		return 0;

	if (recompile_compare_unprefs != NULL
		&& src_a->directory != NULL && src_b->directory != NULL) {

		matcha = match_prefs(src_a->directory, recompile_compare_unprefs);
		matchb = match_prefs(src_b->directory, recompile_compare_unprefs);

		if (matcha > matchb) /* greater is good */
			return -1;
		if (matcha < matchb)
			return 1;
	}

	if (recompile_compare_prefs != NULL
		&& src_a->directory != NULL && src_b->directory != NULL) {

		matcha = match_prefs(src_a->directory, recompile_compare_prefs);
		matchb = match_prefs(src_b->directory, recompile_compare_prefs);

		if (matcha > matchb) /* greater is bad */
			return 1;
		if (matcha < matchb)
			return -1;
	}

	/* else same directory probably */
	foo = strcmp(src_a->name, src_b->name);

	if (foo < 0)
		return -1;
	if (foo > 0)
		return 1;

	return 0;
}

static int
do_recompilation(EhSymTable* table, char* gts_file, unsigned max_globals,
				 char** prefs, char** unprefs,
				 char* rootdir, unsigned verbosity)
{
	SummaryInfo s_info;
	RecompileInfo info;
	unsigned    size;
	unsigned n;
	EhObject* object;
	EhObject** recompiles;
	unsigned delta;
	int rv;
	unsigned nzaps;
	EhObject dummy; /* just marks the end of the recomp list */
	time_t  eta;

	get_summary(table, &s_info);

	if ((s_info.nused + s_info.ndefined) <= max_globals) {
		if (VERBOSITY_USER(verbosity))
			fprintf(stderr,
			"number of globals <= requested max, skipping recompilation\n");
		return 0;
	}

	/* Init recompilation. */
	info.recompile_list = &dummy; /* cannot use NULL, because syms test that */
	info.recompile_count = 0;
	info.recompile_wish_count = 0;
	info.unzap_count = 0;
	info.zap_count = 0;
	EhSymTableMap(table, recompile_init_mappee, (void*)&info);
	size = info.recompile_count;

	recompiles = (EhObject**)malloc(sizeof(EhObject*) * size);

	/* install */
	n = 0;
	for (object = info.recompile_list;
		 object != &dummy;
		 object = object->_recompile) {
		recompiles[n++] = object;
	}

	/* sort */
	recompile_compare_prefs = prefs;
	recompile_compare_unprefs = unprefs;
	qsort(recompiles, size, sizeof(EhObject*), recompile_compare);

	/*
	 *    sorted !
	 *    less recompile the first n, n = ndefined - max
	 */
	delta = (s_info.nused + s_info.ndefined) - max_globals;

	if (delta > info.zap_count) {
		fprintf(stderr,
				"WARNING: there too many globals (%d/%d fixables).\n"
				"         I don't think I can fix this, but I'll try.\n"
				"         You might get lucky.\n",
				info.zap_count,
				delta);
	}

	if (VERBOSITY_USER(verbosity))
		fprintf(stderr, "scheduling recompilation targets:\n");

	eta = 0;
	for (n = 0; n < size; n++) {
		char* cname = "unknown";
		object = recompiles[n];
		if (object->source != NULL) {
			cname = object->source->name;
			eta += object->source->compile_time;
		}
		
		if (VERBOSITY_DEBUG(verbosity))
			fprintf(stderr, "object %s from source %s\n", object->name, cname);
	}

#if 0
	if (VERBOSITY_USER(verbosity))
#endif
		fprintf(stderr, "gts-ing %d symbols, eta = %d minutes\n", delta,
				eta/(60*1000));

	if (gts_file != NULL) {
		FILE* zap_fp;
		if ((zap_fp = fopen(gts_file, "w")) == NULL) {
			perror(0);
			fprintf(stderr,
					"WARNING: could not open the gtscc db file %s.\n"
					"         I will continue with the recompilation, but\n"
					"         if you recompile any of the files I touched\n"
					"         I'll have to recompile them after you!\n",
					gts_file);
		} else {
		
			EhSymTableFpDump(table, zap_fp);
			
			fclose(zap_fp);
		}
	}

	for (n = 0, nzaps = 0; n < size && nzaps < delta; n++) {

		object = recompiles[n];
		rv = EhObjectRebuild(object, table, verbosity, rootdir);

		if (rv == -1)
			return -1;

		nzaps += rv;

		object->_recompile = NULL; /* clean up now */
	}

	if (nzaps < delta) {
		fprintf(stderr,
				"WARNING: I wanted to gts %d symbols, but only managed\n"
				"         to get %d of them.\n"
				"         Your link may fail with GOT errors.\n",
				delta,
				nzaps);
	}
	
	free(recompiles);

	return n;
}

typedef struct FileList
{
	char*            name;
	struct FileList* next;
} FileList;

static FileList*
fileListFind(FileList* list, char* name)
{
	FileList* foo;

	for (foo = list; foo != NULL; foo = foo->next) {
		if (strcmp(name, foo->name) == 0)
			return foo;
	}
	return NULL;
}

static FileList*
fileListAppend(FileList** list_a, char* name)
{
	FileList* list = *list_a;
	FileList* foo;
	FileList* last;

	for (foo = list, last = NULL; foo != NULL; last = foo, foo = foo->next)
		;

	if (last == NULL) {
		foo = EH_NEW(FileList);
		foo->next = NULL;
		foo->name = strdup(name);
		*list_a = foo;
	} else {
		foo = EH_NEW(FileList);
		foo->next = NULL;
		foo->name = strdup(name);
		last->next = foo;
	}

	return *list_a;
}

#if 0
static FileList* c_list;
#endif
static FileList* o_list;

#if 0
static char*
EhSourceAdjustPathname(EhSource* source, char* rootdir)
{
	char buf[MAXPATHLEN];
	char buf2[MAXPATHLEN];
	char* p;
	char* q;
	char* filename = source->name;

	if (getcwd(buf, sizeof(buf)) == NULL) {
		fprintf(stderr, "cannot get pwd\n");
		return NULL;
	}

	strcat(buf, "/");
	strcat(buf, filename);

	if (rootdir == NULL) {
		filename = buf;
	} else {
		if (realpath(buf, buf2) == NULL) {
			fprintf(stderr, "realpath() failed: %s\n", buf2);
			return NULL;
		}

		if (realpath(rootdir, buf) == NULL) {
			fprintf(stderr, "realpath() failed: %s\n", buf);
			return NULL;
		}
		strcat(buf, "/"); 
		
		for (p = buf, q = buf2; *p == *q; p++, q++)
			;

		filename = q;
	}

	free(source->name);
	source->name = strdup(filename);

	return source->name;
}
#endif

static unsigned
katoi(char *buf)
{
	unsigned base = 1;
	char* s;
	
	for (s = buf; isdigit(*s); s++)
		;

	if (*s == 'k' || *s == 'K')
		base = 1024;

	*s = '\0';

	return base * atoi(buf);
}

static void
usage(void)
{
	fprintf(stderr,
			"Usage:\n"
			"as a compiler:\n"
			"gtscc [gtscc_options] [compiler_options] -c file.c file.cpp ...\n"
			"gtscc_options:\n"
			"-gtsfile <db.gts>       the gts database file (use this)\n"
			"-gtszapsymbol <name>    convert symbol <name>\n"
			"-gtsnozapsymbol <name>  don't convert symbol <name>\n"
			"-gtsrootdir <directory> the root for the tree (use this)\n"
			"-gtsverbose             be more verbose (3 levels)\n"
			"-gtsnozap               don't convert globals to statics\n"
			"-gtsnoupdate            don't update the database file\n"
			"as a linker:\n"
			"gtscc [gtscc_options] [linker_options] file.o ... libxx.a ...\n"
			"gtscc_options:\n"
			"-gtsfile <db.gts>       the gts database file (use this)\n"
			"-gtszapsymbol <name>    convert symbol <name>\n"
			"-gtsnozapsymbol <name>  don't convert symbol <name>\n"
			"-gtsrootdir <directory> the root for the tree (use this)\n"
			"-gtspref <directory>    please recompile these paths first\n"
			"-gtsunpref <directory>  please try to avoid recompiling these\n"
			"-gtsverbose             be more verbose (3 levels)\n"
			"-gtssummary             print a summary of global usage\n"
			"-gtsdump                print a detailed listing of all symbols\n"
			"-gtsmaxglobals <number>[k] maximum globals allowed in target\n"
			"-gtsnorecompile         don't do the normal recompilation\n"
			"-gtsnolink              don't call linker after recompilation\n"
			"-gtsnoupdate            don't update the database file\n"
			"-help                   print this\n"
	);
}

int
main(int argc, char** argv)
{
	EhSymTable* table = EhSymTableNew(1000);
	EhSym* sym;
	FILE* zap_fp;
	unsigned n = 0;
	unsigned verbosity = 0;
	char* arg_buf[256];
	unsigned nargs = 0;
	EhDirMap*   dmap = EhDirMapNew();
	unsigned do_dump = 0;
	unsigned do_summary = 0;
	unsigned do_link = 1;
	unsigned in_link = 1;
	unsigned do_audit = 1;
	unsigned do_zap = 1;
	unsigned do_assem = 1;
	unsigned do_recompile = 1;
	unsigned do_collect = 1;
	char* name;
	char* saveprefix = NULL;
	char* rootdir = NULL;
	int rv;
	EhSource* source = NULL;
	char* gts_file = NULL;
	char* path_prefs[32];
	unsigned npath_prefs = 0;
	char* path_un_prefs[32];
	unsigned npath_un_prefs = 0;
	char* suffix;
	unsigned max_globals = DEFAULT_MAX_GLOBALS;
	FileList* list;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "something losing about your elf lib - sorry!\n");
		return 3;
		/* library out of date */
		/* recover from error */
	}

	arg_buf[nargs] = NULL;

	for (n = 1; n < argc; n++) {

		if (strcmp(argv[n], "-help") == 0) {
			usage();
			return 0;
		} else if (strcmp(argv[n], "-gtssummary") == 0) {
			do_summary = 1;
		} else if (strcmp(argv[n], "-gtsdump") == 0) {
			do_dump = 1;
		} else if (strcmp(argv[n], "-gtsnorecompile") == 0) {
			do_recompile = 0;
		} else if (strcmp(argv[n], "-gtsnolink") == 0) {
			do_link = 0;
		} else if (strcmp(argv[n], "-gtsverbose") == 0) {
			verbosity++;
		} else if (strcmp(argv[n], "-gtsnoupdate") == 0) {
			do_collect = 0;
		} else if (strcmp(argv[n], "-gtsnoaudit") == 0) {
			do_audit = 0;
		} else if (strcmp(argv[n], "-gtsnozap") == 0) {
			do_zap = 0;
		} else if (strcmp(argv[n], "-gtsrootdir") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtsrootdir requires an argument\n");
				usage();
				return 2;
			}
			rootdir = argv[n+1];
			n++;
		} else if (strcmp(argv[n], "-gtsdebugsym") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtsdebugsym requires an argument\n");
				usage();
				return 2;
			}
			djw_test_name = argv[n+1];
			n++;
		} else if (strcmp(argv[n], "-gtsmaxglobals") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtsmaxglobals requires an argument\n");
				usage();
				return 2;
			}
			max_globals = katoi(argv[n+1]);
			n++;

		} else if (strcmp(argv[n], "-gtspref") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtspref requires an argument\n");
				usage();
				return 2;
			}
			path_prefs[npath_prefs++] = argv[n+1];
			path_prefs[npath_prefs] = NULL;
			n++;

		} else if (strcmp(argv[n], "-gtsunpref") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtsunpref requires an argument\n");
				usage();
				return 2;
			}
			path_un_prefs[npath_un_prefs++] = argv[n+1];
			path_un_prefs[npath_un_prefs] = NULL;
			n++;

		} else if (strcmp(argv[n], "-gtssaveprefix") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtssaveprefix requires an argument\n");
				usage();
				return 2;
			}
			saveprefix = argv[n+1];
			n++;

		} else if (strcmp(argv[n], "-gtsfile") == 0) {

			struct stat sbuf;

			if (argc < n+2) {
				fprintf(stderr,	"-gtsfile requires an argument\n");
				usage();
				return 2;
			}

			gts_file = argv[n+1];
				
			if (stat(gts_file, &sbuf) == -1) {
				fprintf(stderr,
						"warning: %s does not exist, will be created\n",
						gts_file);
			} else {
				
				if ((zap_fp = fopen(gts_file, "r")) == NULL) {
					fprintf(stderr,	"you lose cannot open %s\n", gts_file);
					usage();
					return 2;
				}

				if (EhSymTableFpLoad(table, zap_fp) == -1) {
					fprintf(stderr,
							"error: failed reading symbols from gtsfile %s\n",
							argv[n+1]);
					usage();
					return 2;
				}
				
				fclose(zap_fp);
			}

			n++;

		} else if (strcmp(argv[n], "-gtszapsymbol") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtszapsymbol requires an argument\n");
				usage();
				return 2;
			}

			EhSymTableSetSymbolState(table, argv[n+1], EH_SYM_ZAPPED);
			n++;

		} else if (strcmp(argv[n], "-gtsnozapsymbol") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtsnozapsymbol requires an argument\n");
				usage();
				return 2;
			}

			EhSymTableSetSymbolState(table, argv[n+1], EH_SYM_DEFINED);
			n++;

		} else if (strcmp(argv[n], "-gtsname") == 0) {
			if (argc < n+2) {
				fprintf(stderr,	"-gtsname requires an argument\n");
				usage();
				return 2;
			}

			sym = EhSymTableFind(table, argv[n+1]);
			if (!sym)
				sym = EhSymNewRandomZap(argv[n+1]);
			n++;
			do_audit = 1;

		} else if (strcmp(argv[n], "-c") == 0) { /* do not link */
			in_link = 0;
			do_link = 0;
		} else if (strcmp(argv[n], "-S") == 0) { /* do not assem */
			do_assem = 0;
		} else if (strcmp(argv[n], "-o") == 0) { /* parse through */
			arg_buf[nargs++] = argv[n++];
			arg_buf[nargs++] = argv[n];
			arg_buf[nargs] = NULL;
		} else if (strcmp((suffix = suffix_name(argv[n])), ".cpp") == 0
				   ||
				   strcmp(suffix, ".c") == 0
				   ||
				   strcmp(suffix, ".s") == 0) {
			char pathname[MAXPATHLEN];

			make_relative_pathname(pathname, ".", rootdir);

			source = EhSourceNew(argv[n], arg_buf, pathname);

			rv = EhSourceCompile(source,
								 table,
								 verbosity,
								 TRUE, /* compile, .s files ignore anyway */
								 (do_audit + do_zap),
								 do_assem);
			if (rv == -1)
				return 1;

#if 0
			EhSourceAdjustPathname(source, rootdir);
#endif

		} else if (strcmp(suffix, ".o") == 0 || strcmp(suffix, ".a") == 0) {

			if (fileListFind(o_list, argv[n]) == NULL) {
				fileListAppend(&o_list, argv[n]);
			} else {
				fprintf(stderr,
						"%s repeated on command line - ignored\n",
						argv[n]);
			}
			arg_buf[nargs++] = argv[n];
			arg_buf[nargs] = NULL;

		} else if (strncmp(argv[n], "-L", 2) == 0) {
			EhDirMapAddDirectory(dmap, &argv[n][2]);
		} else if (strncmp(argv[n], "-l", 2) == 0) {
			char pathbuf[MAXPATHLEN];
			name = EhDirMapGetLibName(dmap, &argv[n][2], pathbuf);
			if (name != NULL) {
				if (fileListFind(o_list, name) == NULL) {
					fileListAppend(&o_list, name);
				} else {
					fprintf(stderr,
							"%s repeated on command line - ignored\n",
							name);
				}
			} else {
				fprintf(stderr, 
						"unable to resolve library reference %s - ignoring\n",
						argv[n]);
			}
			arg_buf[nargs++] = argv[n];
			arg_buf[nargs] = NULL;
		} else {
			arg_buf[nargs++] = argv[n];
			arg_buf[nargs] = NULL;
		}
	}

	/*
	 *    Analyse objects.
	 */
	if (o_list != NULL) {
		for (list = o_list; list != NULL; list = list->next) {

			if (eh_process_file(list->name, table, rootdir)) {
				fprintf(stderr, "oops we died around %s\n", list->name);
				return 1;
			}
		}
		
		/* look for unused objects */
		EhSymTableMap(table, object_nusers_mappee, 0);
	}

	if (do_summary) {
		print_summary(table);
	}

	if (do_dump) {
		print_dump(table);
	}

	if (!in_link && gts_file != NULL) {
		FILE* zap_fp;
		if ((zap_fp = fopen(gts_file, "w")) == NULL) {
			perror(0);
			usage();
			return 2;
		}

		EhSymTableFpDump(table, zap_fp);
		fclose(zap_fp);
		return 0;
	}

	/*
	 *    Now the fun really starts.
	 */
	if (do_recompile) {
		char** pp = NULL;
		char** up = NULL;
		
		if (npath_prefs > 0)
			pp = path_prefs;
		
		if (npath_un_prefs > 0)
			up = path_un_prefs;

		if (!do_collect)
			gts_file = NULL;
		
		rv = do_recompilation(table, gts_file, max_globals, pp, up, rootdir,
							  verbosity);
		if (rv == -1)
			return 1;
	}

	/*
	 *    Finally.
	 */
	if (do_link) {

		int status;

		arg_buf[nargs+1] = NULL;
		for (n = nargs; n > 0; n--)
			arg_buf[n] = arg_buf[n-1];
		arg_buf[0] = LD_COMMAND;
		
		status = do_command("link", arg_buf, verbosity);

		if (status == -1)
			return 3;
		else
			return status;
	}
	
	return 0;
}
