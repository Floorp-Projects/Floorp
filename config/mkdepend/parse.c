/* $Xorg: parse.c,v 1.6 2001/02/09 02:03:16 xorgcvs Exp $ */
/*

Copyright (c) 1993, 1994, 1998 The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

#include "def.h"

extern char	*directives[];
extern struct inclist	maininclist;

int
gobble(filep, file, file_red)
	register struct filepointer *filep;
	struct inclist		*file, *file_red;
{
	register char	*line;
	register int	type;

	while ((line = getline(filep))) {
		switch(type = deftype(line, filep, file_red, file, FALSE)) {
		case IF:
		case IFFALSE:
		case IFGUESSFALSE:
		case IFDEF:
		case IFNDEF:
			type = gobble(filep, file, file_red);
			while ((type == ELIF) || (type == ELIFFALSE) ||
			       (type == ELIFGUESSFALSE))
			    type = gobble(filep, file, file_red);
			if (type == ELSE)
			        (void)gobble(filep, file, file_red);
			break;
		case ELSE:
		case ENDIF:
			debug(0,("%s, line %d: #%s\n",
				file->i_file, filep->f_line,
				directives[type]));
			return(type);
		case DEFINE:
		case UNDEF:
		case INCLUDE:
		case INCLUDEDOT:
		case PRAGMA:
		case ERROR:
		case IDENT:
		case SCCS:
		case EJECT:
		case WARNING:
			break;
		case ELIF:
		case ELIFFALSE:
		case ELIFGUESSFALSE:
			return(type);
		case -1:
			warning("%s", file_red->i_file);
			if (file_red != file)
				warning1(" (reading %s)", file->i_file);
			warning1(", line %d: unknown directive == \"%s\"\n",
				filep->f_line, line);
			break;
		}
	}
	return(-1);
}

/*
 * Decide what type of # directive this line is.
 */
int deftype (line, filep, file_red, file, parse_it)
	register char	*line;
	register struct filepointer *filep;
	register struct inclist *file_red, *file;
	int	parse_it;
{
	register char	*p;
	char	*directive, savechar;
	register int	ret;

	/*
	 * Parse the directive...
	 */
	directive=line+1;
	while (*directive == ' ' || *directive == '\t')
		directive++;

	p = directive;
	while (*p >= 'a' && *p <= 'z')
		p++;
	savechar = *p;
	*p = '\0';
	ret = match(directive, directives);
	*p = savechar;

	/* If we don't recognize this compiler directive or we happen to just
	 * be gobbling up text while waiting for an #endif or #elif or #else
	 * in the case of an #elif we must check the zero_value and return an
	 * ELIF or an ELIFFALSE.
	 */

	if (ret == ELIF && !parse_it)
	{
	    while (*p == ' ' || *p == '\t')
		p++;
	    /*
	     * parse an expression.
	     */
	    debug(0,("%s, line %d: #elif %s ",
		   file->i_file, filep->f_line, p));
	    ret = zero_value(file->i_file, p, filep, file_red);
	    if (ret != IF)
	    {
		debug(0,("false...\n"));
		if (ret == IFFALSE)
		    return(ELIFFALSE);
		else
		    return(ELIFGUESSFALSE);
	    }
	    else
	    {
		debug(0,("true...\n"));
		return(ELIF);
	    }
	}

	if (ret < 0 || ! parse_it)
		return(ret);

	/*
	 * now decide how to parse the directive, and do it.
	 */
	while (*p == ' ' || *p == '\t')
		p++;
	switch (ret) {
	case IF:
		/*
		 * parse an expression.
		 */
		ret = zero_value(file->i_file, p, filep, file_red);
		debug(0,("%s, line %d: %s #if %s\n",
			 file->i_file, filep->f_line, ret?"false":"true", p));
		break;
	case IFDEF:
	case IFNDEF:
		debug(0,("%s, line %d: #%s %s\n",
			file->i_file, filep->f_line, directives[ret], p));
	case UNDEF:
		/*
		 * separate the name of a single symbol.
		 */
		while (isalnum(*p) || *p == '_')
			*line++ = *p++;
		*line = '\0';
		break;
	case INCLUDE:
		debug(2,("%s, line %d: #include %s\n",
			file->i_file, filep->f_line, p));

		/* Support ANSI macro substitution */
		{
		    struct symtab **sym = isdefined(p, file_red, NULL);
		    while (sym) {
			p = (*sym)->s_value;
			debug(3,("%s : #includes SYMBOL %s = %s\n",
			       file->i_incstring,
			       (*sym) -> s_name,
			       (*sym) -> s_value));
			/* mark file as having included a 'soft include' */
			file->i_flags |= INCLUDED_SYM; 
			sym = isdefined(p, file_red, NULL);
		    }
		}

		/*
		 * Separate the name of the include file.
		 */
		while (*p && *p != '"' && *p != '<')
			p++;
		if (! *p)
			return(-2);
		if (*p++ == '"') {
			ret = INCLUDEDOT;
			while (*p && *p != '"')
				*line++ = *p++;
		} else
			while (*p && *p != '>')
				*line++ = *p++;
		*line = '\0';
		break;
	case DEFINE:
		/*
		 * copy the definition back to the beginning of the line.
		 */
		strcpy (line, p);
		break;
	case ELSE:
	case ENDIF:
	case ELIF:
	case PRAGMA:
	case ERROR:
	case IDENT:
	case SCCS:
	case EJECT:
		debug(0,("%s, line %d: #%s\n",
			file->i_file, filep->f_line, directives[ret]));
		/*
		 * nothing to do.
		 */
		break;
	}
	return(ret);
}

struct symtab **fdefined(symbol, file, srcfile)
	register char	*symbol;
	struct inclist	*file;
	struct inclist	**srcfile;
{
	register struct inclist	**ip;
	register struct symtab	**val;
	register int	i;
	static int	recurse_lvl = 0;

	if (file->i_flags & DEFCHECKED)
		return(NULL);
	file->i_flags |= DEFCHECKED;
	if ((val = slookup(symbol, file)))
		debug(1,("%s defined in %s as %s\n",
			 symbol, file->i_file, (*val)->s_value));
	if (val == NULL && file->i_list)
		{
		for (ip = file->i_list, i=0; i < file->i_listlen; i++, ip++)
			if (file->i_merged[i]==FALSE) {
				val = fdefined(symbol, *ip, srcfile);
				if ((*ip)->i_flags & FINISHED) {
					merge2defines(file,*ip);
					file->i_merged[i]=TRUE;
				    }
				if (val!=NULL) break;
			}
		}
	else if (val != NULL && srcfile != NULL) *srcfile = file;
	recurse_lvl--;
	file->i_flags &= ~DEFCHECKED;

	return(val);
}

struct symtab **isdefined(symbol, file, srcfile)
	register char	*symbol;
	struct inclist	*file;
	struct inclist	**srcfile;
{
	register struct symtab	**val;

	if ((val = slookup(symbol, &maininclist))) {
		debug(1,("%s defined on command line\n", symbol));
		if (srcfile != NULL) *srcfile = &maininclist;
		return(val);
	}
	if ((val = fdefined(symbol, file, srcfile)))
		return(val);
	debug(1,("%s not defined in %s\n", symbol, file->i_file));
	return(NULL);
}

/*
 * Return type based on if the #if expression evaluates to 0
 */
int
zero_value(filename, exp, filep, file_red)
	register char   *filename;
	register char	*exp;
	register struct filepointer *filep;
	register struct inclist *file_red;
{
	if (cppsetup(filename, exp, filep, file_red))
	    return(IFFALSE);
	else
	    return(IF);
}

void
define2(name, val, file)
	char	*name, *val;
	struct inclist	*file;
{
    int first, last, below;
    register struct symtab **sp = NULL, **dest;
    struct symtab *stab;

    /* Make space if it's needed */
    if (file->i_defs == NULL)
    {
	file->i_defs = (struct symtab **)
			malloc(sizeof (struct symtab*) * SYMTABINC);
	file->i_ndefs = 0;
    }
    else if (!(file->i_ndefs % SYMTABINC))
	file->i_defs = (struct symtab **)
			realloc(file->i_defs,
			   sizeof(struct symtab*)*(file->i_ndefs+SYMTABINC));

    if (file->i_defs == NULL)
	fatalerr("malloc()/realloc() failure in insert_defn()\n");

    below = first = 0;
    last = file->i_ndefs - 1;
    while (last >= first)
    {
	/* Fast inline binary search */
	register char *s1;
	register char *s2;
	register int middle = (first + last) / 2;

	/* Fast inline strchr() */
	s1 = name;
	s2 = file->i_defs[middle]->s_name;
	while (*s1++ == *s2++)
	    if (s2[-1] == '\0') break;

	/* If exact match, set sp and break */
	if (*--s1 == *--s2) 
	{
	    sp = file->i_defs + middle;
	    break;
	}

	/* If name > i_defs[middle] ... */
	if (*s1 > *s2) 
	{
	    below = first;
	    first = middle + 1;
	}
	/* else ... */
	else
	{
	    below = last = middle - 1;
	}
    }

    /* Search is done.  If we found an exact match to the symbol name,
       just replace its s_value */
    if (sp != NULL)
    {
	free((*sp)->s_value);
	(*sp)->s_value = copy(val);
	return;
    }

    sp = file->i_defs + file->i_ndefs++;
    dest = file->i_defs + below + 1;
    while (sp > dest)
    {
	*sp = sp[-1];
	sp--;
    }
    stab = (struct symtab *) malloc(sizeof (struct symtab));
    if (stab == NULL)
	fatalerr("malloc()/realloc() failure in insert_defn()\n");

    stab->s_name = copy(name);
    stab->s_value = copy(val);
    *sp = stab;
}

void
define(def, file)
	char	*def;
	struct inclist	*file;
{
    char *val;

    /* Separate symbol name and its value */
    val = def;
    while (isalnum(*val) || *val == '_')
	val++;
    if (*val)
	*val++ = '\0';
    while (*val == ' ' || *val == '\t')
	val++;

    if (!*val)
	val = "1";
    define2(def, val, file);
}

struct symtab **slookup(symbol, file)
	register char	*symbol;
	register struct inclist	*file;
{
	register int first = 0;
	register int last = file->i_ndefs - 1;

	if (file) while (last >= first)
	{
	    /* Fast inline binary search */
	    register char *s1;
	    register char *s2;
	    register int middle = (first + last) / 2;

	    /* Fast inline strchr() */
	    s1 = symbol;
	    s2 = file->i_defs[middle]->s_name;
	    while (*s1++ == *s2++)
	        if (s2[-1] == '\0') break;

	    /* If exact match, we're done */
	    if (*--s1 == *--s2) 
	    {
	        return file->i_defs + middle;
	    }

	    /* If symbol > i_defs[middle] ... */
	    if (*s1 > *s2) 
	    {
	        first = middle + 1;
	    }
	    /* else ... */
	    else
	    {
	        last = middle - 1;
	    }
	}
	return(NULL);
}

int merge2defines(file1, file2)
	struct inclist	*file1;
	struct inclist	*file2;
{
	if ((file1!=NULL) && (file2!=NULL)) 
        {
		int first1 = 0;
		int last1 = file1->i_ndefs - 1;

		int first2 = 0;
		int last2 = file2->i_ndefs - 1;

                int first=0;
                struct symtab** i_defs = NULL;
		int deflen=file1->i_ndefs+file2->i_ndefs;

                if (deflen>0)
                { 
                	/* make sure deflen % SYMTABINC == 0 is still true */
                	deflen += (SYMTABINC - deflen % SYMTABINC) % SYMTABINC;
                	i_defs=(struct symtab**)
			    malloc(deflen*sizeof(struct symtab*));
                	if (i_defs==NULL) return 0;
        	}

        	while ((last1 >= first1) && (last2 >= first2))
        	{
	    		char *s1=file1->i_defs[first1]->s_name;
	    		char *s2=file2->i_defs[first2]->s_name;

     			if (strcmp(s1,s2) < 0)
                        	i_defs[first++]=file1->i_defs[first1++];
     			else if (strcmp(s1,s2) > 0)
                        	i_defs[first++]=file2->i_defs[first2++];
                        else /* equal */
                        {
                        	i_defs[first++]=file2->i_defs[first2++];
                                first1++;
                        }
        	}
        	while (last1 >= first1)
        	{
                        i_defs[first++]=file1->i_defs[first1++];
        	}
        	while (last2 >= first2)
        	{
                        i_defs[first++]=file2->i_defs[first2++];
        	}

                if (file1->i_defs) free(file1->i_defs);
                file1->i_defs=i_defs;
                file1->i_ndefs=first;
                
		return 1;
  	}
	return 0;
}

void
undefine(symbol, file)
	char	*symbol;
	register struct inclist	*file;
{
	register struct symtab **ptr;
	struct inclist *srcfile;
	while ((ptr = isdefined(symbol, file, &srcfile)) != NULL)
	{
	    srcfile->i_ndefs--;
	    for (; ptr < srcfile->i_defs + srcfile->i_ndefs; ptr++)
		*ptr = ptr[1];
	}
}

int
find_includes(filep, file, file_red, recursion, failOK)
	struct filepointer	*filep;
	struct inclist		*file, *file_red;
	int			recursion;
	boolean			failOK;
{
	register char	*line;
	register int	type;
	boolean recfailOK;

	while ((line = getline(filep))) {
		switch(type = deftype(line, filep, file_red, file, TRUE)) {
		case IF:
		doif:
			type = find_includes(filep, file,
				file_red, recursion+1, failOK);
			while ((type == ELIF) || (type == ELIFFALSE) ||
			       (type == ELIFGUESSFALSE))
				type = gobble(filep, file, file_red);
			if (type == ELSE)
				gobble(filep, file, file_red);
			break;
		case IFFALSE:
		case IFGUESSFALSE:
		    doiffalse:
			if (type == IFGUESSFALSE || type == ELIFGUESSFALSE)
			    recfailOK = TRUE;
			else
			    recfailOK = failOK;
			type = gobble(filep, file, file_red);
			if (type == ELSE)
			    find_includes(filep, file,
					  file_red, recursion+1, recfailOK);
			else
			if (type == ELIF)
			    goto doif;
			else
			if ((type == ELIFFALSE) || (type == ELIFGUESSFALSE))
			    goto doiffalse;
			break;
		case IFDEF:
		case IFNDEF:
			if ((type == IFDEF && isdefined(line, file_red, NULL))
			 || (type == IFNDEF && !isdefined(line, file_red, NULL))) {
				debug(1,(type == IFNDEF ?
				    "line %d: %s !def'd in %s via %s%s\n" : "",
				    filep->f_line, line,
				    file->i_file, file_red->i_file, ": doit"));
				type = find_includes(filep, file,
					file_red, recursion+1, failOK);
				while (type == ELIF || type == ELIFFALSE || type == ELIFGUESSFALSE)
					type = gobble(filep, file, file_red);
				if (type == ELSE)
					gobble(filep, file, file_red);
			}
			else {
				debug(1,(type == IFDEF ?
				    "line %d: %s !def'd in %s via %s%s\n" : "",
				    filep->f_line, line,
				    file->i_file, file_red->i_file, ": gobble"));
				type = gobble(filep, file, file_red);
				if (type == ELSE)
					find_includes(filep, file,
						file_red, recursion+1, failOK);
				else if (type == ELIF)
				    	goto doif;
				else if (type == ELIFFALSE || type == ELIFGUESSFALSE)
				    	goto doiffalse;
			}
			break;
		case ELSE:
		case ELIFFALSE:
		case ELIFGUESSFALSE:
		case ELIF:
			if (!recursion)
				gobble(filep, file, file_red);
		case ENDIF:
			if (recursion)
				return(type);
		case DEFINE:
			define(line, file);
			break;
		case UNDEF:
			if (!*line) {
			    warning("%s", file_red->i_file);
			    if (file_red != file)
				warning1(" (reading %s)", file->i_file);
			    warning1(", line %d: incomplete undef == \"%s\"\n",
				filep->f_line, line);
			    break;
			}
			undefine(line, file_red);
			break;
		case INCLUDE:
			add_include(filep, file, file_red, line, FALSE, failOK);
			break;
		case INCLUDEDOT:
			add_include(filep, file, file_red, line, TRUE, failOK);
			break;
		case ERROR:
		    	warning("%s", file_red->i_file);
			if (file_red != file)
				warning1(" (reading %s)", file->i_file);
			warning1(", line %d: %s\n",
				 filep->f_line, line);
		    	break;
		    
		case PRAGMA:
		case IDENT:
		case SCCS:
		case EJECT:
			break;
		case -1:
			warning("%s", file_red->i_file);
			if (file_red != file)
			    warning1(" (reading %s)", file->i_file);
			warning1(", line %d: unknown directive == \"%s\"\n",
				 filep->f_line, line);
			break;
		case -2:
			warning("%s", file_red->i_file);
			if (file_red != file)
			    warning1(" (reading %s)", file->i_file);
			warning1(", line %d: incomplete include == \"%s\"\n",
				 filep->f_line, line);
			break;
		}
	}
	file->i_flags |= FINISHED;
	return(-1);
}
