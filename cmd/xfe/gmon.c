/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Mangled into a form that works on Sparc Solaris 2 by Mark Eichin
 * for Cygnus Support, July 1992.
 */

/*
 *    Largely hacked, ANSI-afied and re-written for use at Netscape
 *    by djw@netscape.com.
 *    Algorithms and code are based on BSD gmon.c, rev 5.3.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <mon.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#if 0
#include "sparc/gmon.h"
#else
struct phdr {
  char *lpc;
  char *hpc;
  int ncnt;
};
#define HISTFRACTION 2
#define HISTCOUNTER  unsigned short
#define HASHFRACTION 1
#define ARCDENSITY   2
#define MINARCS      50
struct tostruct {
  char *selfpc;
  long count;
  unsigned short link;
};
struct rawarc {
    unsigned long       raw_frompc;
    unsigned long       raw_selfpc;
    long                raw_count;
};
#define ROUNDDOWN(x,y)  (((x)/(y))*(y))
#define ROUNDUP(x,y)    ((((x)+(y)-1)/(y))*(y))

#endif

/* extern mcount() asm ("mcount"); */
/*extern*/ char *minbrk /* asm ("minbrk") */;

/*
 *	froms is actually a bunch of unsigned shorts indexing tos
 */
static int		        profiling = 3;
static unsigned short*  froms;
static struct tostruct* tos = 0;
static long		        tolimit = 0;
static char*            s_lowpc = 0;
static char*            s_highpc = 0;
static unsigned long	s_textsize = 0;

static int	            ssiz;
static char*            sbuf;
static int              s_scale;

/* see profil(2) where this is describe (incorrectly) */
#define		SCALE_1_TO_1	0x10000L

#define	MSG "No space for profiling buffer(s)\n"

static void moncontrol();

static void
monstartup(char* lowpc, char* highpc)
{
    int			monsize;
    char		*buffer;
    register int	o;

	/*
	 *	round lowpc and highpc to multiples of the density we're using
	 *	so the rest of the scaling (here and in gprof) stays in ints.
	 */
    lowpc = (char *)
	    ROUNDDOWN((unsigned)lowpc, HISTFRACTION*sizeof(HISTCOUNTER));
    s_lowpc = lowpc;
    highpc = (char *)
	    ROUNDUP((unsigned)highpc, HISTFRACTION*sizeof(HISTCOUNTER));
    s_highpc = highpc;
    s_textsize = highpc - lowpc;
    monsize = (s_textsize / HISTFRACTION) + sizeof(struct phdr);
    buffer = sbrk( monsize );
    if ( buffer == (char *) -1 ) {
		write( 2 , MSG , sizeof(MSG) );
		return;
    }
    froms = (unsigned short *) sbrk( s_textsize / HASHFRACTION );
    if ( froms == (unsigned short *) -1 ) {
		write( 2 , MSG , sizeof(MSG) );
		froms = 0;
		return;
    }
    tolimit = s_textsize * ARCDENSITY / 100;
    if ( tolimit < MINARCS ) {
		tolimit = MINARCS;
    } else if ( tolimit > 65534 ) {
		tolimit = 65534;
    }
    tos = (struct tostruct *) sbrk( tolimit * sizeof( struct tostruct ) );
    if ( tos == (struct tostruct *) -1 ) {
		write( 2 , MSG , sizeof(MSG) );
		froms = 0;
		tos = 0;
		return;
    }
    minbrk = sbrk(0);
    tos[0].link = 0;
    sbuf = buffer;
    ssiz = monsize;
    ( (struct phdr *) buffer ) -> lpc = lowpc;
    ( (struct phdr *) buffer ) -> hpc = highpc;
    ( (struct phdr *) buffer ) -> ncnt = ssiz;
    monsize -= sizeof(struct phdr);
    if ( monsize <= 0 )
		return;
    o = highpc - lowpc;
    if( monsize < o )
#ifndef hp300
		s_scale = ( (float) monsize / o ) * SCALE_1_TO_1;
#else /* avoid floating point */
    {
		int quot = o / monsize;
		
		if (quot >= 0x10000)
			s_scale = 1;
		else if (quot >= 0x100)
			s_scale = 0x10000 / quot;
		else if (o >= 0x800000)
			s_scale = 0x1000000 / (o / (monsize >> 8));
		else
			s_scale = 0x1000000 / ((o << 8) / monsize);
    }
#endif
    else
		s_scale = SCALE_1_TO_1;
#if 0
    moncontrol(1);
#endif
}

static void
_mcleanup(char* filename)
{
    int			fd;
    int			fromindex;
    int			endfrom;
    char		*frompc;
    int			toindex;
    struct rawarc	rawarc;

    moncontrol(0);
    fd = creat( filename , 0666 );
    if ( fd < 0 ) {
		perror( "mcount: gmon.out" );
		return;
    }
#   ifdef DEBUG
	fprintf( stderr , "[mcleanup] sbuf 0x%x ssiz %d\n" , sbuf , ssiz );
#   endif DEBUG
    write( fd , sbuf , ssiz );
    endfrom = s_textsize / (HASHFRACTION * sizeof(*froms));
    for ( fromindex = 0 ; fromindex < endfrom ; fromindex++ ) {
		if ( froms[fromindex] == 0 ) {
			continue;
		}
		frompc = s_lowpc + (fromindex * HASHFRACTION * sizeof(*froms));
		for (toindex=froms[fromindex]; toindex!=0; toindex=tos[toindex].link) {
#	    ifdef DEBUG
			fprintf( stderr ,
					 "[mcleanup] frompc 0x%x selfpc 0x%x count %d\n" ,
					 frompc , tos[toindex].selfpc , tos[toindex].count );
#	    endif DEBUG
			rawarc.raw_frompc = (unsigned long) frompc;
			rawarc.raw_selfpc = (unsigned long) tos[toindex].selfpc;
			rawarc.raw_count = tos[toindex].count;
			write( fd , &rawarc , sizeof rawarc );
		}
    }
    close( fd );
}

/*
 * The Sparc stack frame is only held together by the frame pointers
 * in the register windows. According to the SVR4 SPARC ABI
 * Supplement, Low Level System Information/Operating System
 * Interface/Software Trap Types, a type 3 trap will flush all of the
 * register windows to the stack, which will make it possible to walk
 * the frames and find the return addresses.
 * 	However, it seems awfully expensive to incur a trap (system
 * call) for every function call. It turns out that "call" simply puts
 * the return address in %o7 expecting the "save" in the procedure to
 * shift it into %i7; this means that before the "save" occurs, %o7
 * contains the address of the call to mcount, and %i7 still contains
 * the caller above that. The asm mcount here simply saves those
 * registers in argument registers and branches to internal_mcount,
 * simulating a call with arguments.
 * 	Kludges:
 * 	1) the branch to internal_mcount is hard coded; it should be
 * possible to tell asm to use the assembler-name of a symbol.
 * 	2) in theory, the function calling mcount could have saved %i7
 * somewhere and reused the register; in practice, I *think* this will
 * break longjmp (and maybe the debugger) but I'm not certain. (I take
 * some comfort in the knowledge that it will break the native mcount
 * as well.)
 * 	3) if builtin_return_address worked, this could be portable.
 * However, it would really have to be optimized for arguments of 0
 * and 1 and do something like what we have here in order to avoid the
 * trap per function call performance hit. 
 * 	4) the atexit and monsetup calls prevent this from simply
 * being a leaf routine that doesn't do a "save" (and would thus have
 * access to %o7 and %i7 directly) but the call to write() at the end
 * would have also prevented this.
 *
 * -- [eichin:19920702.1107EST]
 */

/* i7 == last ret, -> frompcindex */
/* o7 == current ret, -> selfpc */
asm(".global mcount; mcount: mov %i7,%o1; mov %o7,%o0;b,a internal_mcount");

static void
internal_mcount(register char* selfpc, register unsigned short* frompcindex)
{
	register struct tostruct	*top;
	register struct tostruct	*prevtop;
	register long			toindex;
	/*
	 *	check that we are profiling
	 *	and that we aren't recursively invoked.
	 */
	if (profiling) {
		goto out;
	}
	profiling++;
	/*
	 *	check that frompcindex is a reasonable pc value.
	 *	for example:	signal catchers get called from the stack,
	 *			not from text space.  too bad.
	 */
	frompcindex = (unsigned short *)((long)frompcindex - (long)s_lowpc);
	if ((unsigned long)frompcindex > s_textsize) {
		goto done;
	}
	frompcindex =
	    &froms[((long)frompcindex) / (HASHFRACTION * sizeof(*froms))];
	toindex = *frompcindex;
	if (toindex == 0) {
		/*
		 *	first time traversing this arc
		 */
		toindex = ++tos[0].link;
		if (toindex >= tolimit) {
			goto overflow;
		}
		*frompcindex = toindex;
		top = &tos[toindex];
		top->selfpc = selfpc;
		top->count = 1;
		top->link = 0;
		goto done;
	}
	top = &tos[toindex];
	if (top->selfpc == selfpc) {
		/*
		 *	arc at front of chain; usual case.
		 */
		top->count++;
		goto done;
	}
	/*
	 *	have to go looking down chain for it.
	 *	top points to what we are looking at,
	 *	prevtop points to previous top.
	 *	we know it is not at the head of the chain.
	 */
	for (; /* goto done */; ) {
		if (top->link == 0) {
			/*
			 *	top is end of the chain and none of the chain
			 *	had top->selfpc == selfpc.
			 *	so we allocate a new tostruct
			 *	and link it to the head of the chain.
			 */
			toindex = ++tos[0].link;
			if (toindex >= tolimit) {
				goto overflow;
			}
			top = &tos[toindex];
			top->selfpc = selfpc;
			top->count = 1;
			top->link = *frompcindex;
			*frompcindex = toindex;
			goto done;
		}
		/*
		 *	otherwise, check the next arc on the chain.
		 */
		prevtop = top;
		top = &tos[top->link];
		if (top->selfpc == selfpc) {
			/*
			 *	there it is.
			 *	increment its count
			 *	move it to the head of the chain.
			 */
			top->count++;
			toindex = prevtop->link;
			prevtop->link = top->link;
			top->link = *frompcindex;
			*frompcindex = toindex;
			goto done;
		}

	}
done:
	profiling--;
	/* and fall through */
out:
	return;		/* normal return restores saved registers */

overflow:
	profiling++; /* halt further profiling */
#   define	TOLIMIT	"mcount: tos overflow\n"
	write(2, TOLIMIT, sizeof(TOLIMIT));
	goto out;
}

/*
 * Control profiling
 *	profiling is what mcount checks to see if
 *	all the data structures are ready.
 */
static void
moncontrol(int mode)
{
    if (mode) {
		/* start */
		profil((unsigned short *)(sbuf + sizeof(struct phdr)),
			   ssiz - sizeof(struct phdr),
			   (int)s_lowpc, s_scale);
		profiling = 0;
    } else {
		/* stop */
		profil((unsigned short *)0, 0, 0, 0);
		profiling = 3;
    }
}

/*
 *    Mozilla-isms:
 */

/*
 *    Deal with nspr system traps.
 */
extern int _pr_intsOff; /* in nspr */

#define CONTROL_VARIABLE "MOZILLA_GPROF"
#define DEFAULT_NCALLS   10000
#define DEFAULT_RUNNING  (1)
#define DEFAULT_DATAFILE "gmon.out"
#define BARSIZE          8 /* same as ld.so does */

typedef int (*funcptr_t)();

extern _etext;

#define PROG_LOW  ((funcptr_t)0)
#define PROG_HIGH ((funcptr_t)&_etext)

static unsigned gmon_running;
#if 0
static WORD*    gmon_buf;
static size_t   gmon_nelems;
#endif
static size_t   gmon_ncalls;
static char*    gmon_filename;

static void
gmon_parse_env(char* s, size_t* ncalls_r, unsigned* running_r, char** file)
{
	char* p = (char*)&internal_mcount; /* just so it gets used somewhere */
	char* name;
	char* value;
	char  buf[1024]; /* big enough to hold a long filename */

	if (s == NULL)
		return; /* no change */

	strcpy(buf, s);

	for (p = buf; *p != '\0';) {
		while (*p != '\0' && isspace(*p)) /* skip white */
			;
		if (*p == '\0')
			break;

		name = p;

		if ((p = strchr(name, '=')) == NULL)
			break;
		*p++ = '\0';

		value = p;

		if ((p = strchr(name, ';')) != NULL) {
			*p++ = '\0';
		} else {
			for (p = name; *p != '\0'; p++)
				;
		}

		if (strncmp(name, "NCALLS", 6) == 0) {
			unsigned foo = atoi(value);
			*ncalls_r = foo;
		} else if (strncmp(name, "RUNNING", 7) == 0) {
			if (strcmp(name, "true") == 0)
				*running_r = 1;
			else
				*running_r = 0;
		} else if (strncmp(name, "FILE", 4) == 0) {
			*file = strdup(value);
		}
	}
}

static void
gmon_change_state(unsigned running)
{
	int s;
	/*
	 *    At the moment, dump_to() doesn't work. Just always dump to
	 *    gmon.out.
	 */
    s = _pr_intsOff; /* stop annoying assert from nspr */
    _pr_intsOff = 0;

	moncontrol(running);

	_pr_intsOff = s;

	gmon_running = running;
}

void gmon_dump();

static void
gmon_onexit_cb()
{
	if (gmon_running)
		gmon_dump();
}


void
gmon_init()
{
	char*     p = getenv(CONTROL_VARIABLE);

	gmon_ncalls = DEFAULT_NCALLS;
	gmon_filename = DEFAULT_DATAFILE;
	gmon_running = DEFAULT_RUNNING;

	if (p != NULL)
		gmon_parse_env(p, &gmon_ncalls, &gmon_running, &gmon_filename);

#if 0
	size_t    bufsize;
	bufsize =
		sizeof(struct hdr) + 
		(gmon_ncalls * sizeof(struct cnt)) +
		(((PROG_HIGH - PROG_LOW)/BARSIZE) * sizeof(WORD)) +
		sizeof(WORD) - 1;
	
	gmon_nelems = (bufsize / sizeof(WORD));

	gmon_buf = (WORD*)malloc(bufsize);

	if (!gmon_buf) {
		fprintf(stderr,
				"gmon_init() failed to allocate data buffers.\n"
				"Sorry, I just cannot continue like this. How about\n"
				"checking " CONTROL_VARIABLE ", currently set to:\n"
				CONTROL_VARIABLE "=" "%s\n", (p != NULL)? p: "unset");
		exit(3);
	}
#endif

	atexit(gmon_onexit_cb);

	monstartup((char*)PROG_LOW, (char*)PROG_HIGH);

	gmon_change_state(gmon_running);
}

void
gmon_start()
{
	gmon_change_state(1);
}

void
gmon_stop()
{
	gmon_change_state(0);
}

void
gmon_dump_to(char* name)
{
	int s;
	/*
	 *    At the moment, dump_to() doesn't work. Just always dump to
	 *    gmon.out.
	 */
    s = _pr_intsOff; /* stop annoying assert from nspr */
    _pr_intsOff = 0;

	_mcleanup(name);

    _pr_intsOff = s;
}

void
gmon_dump()
{
	gmon_dump_to(gmon_filename);
}

static int
gprof_out_filter_section_1(FILE* ifp, FILE* ofp)
{
	char buf[512];
	char* p;
	char* q;
	char* rest;
	unsigned link;
	unsigned target;

	while (fgets(buf, sizeof(buf), ifp) != NULL) {

		if (buf[0] == '\f')
			break; /* done for this section */

		if ((p = strchr(buf, '[')) == NULL) { /* heading or data line */
			fputs(buf, ofp); /* white space, etc */
			continue;
		}

		target = 0;
		rest = buf;
		if (p == buf && (q = strchr(p+1, '[')) != NULL) { /* heading line */
			target = strtol(p+1, &rest, 10);
			if (*rest == ']')
				rest++;
			p = q;
		}

		link = atoi(p+1);
		*p = '\0';
		
		if (rest != buf) {
			fprintf(ofp,
					"<A NAME=s1i%d>[%d]</A>%s<A HREF=#s2i%d>[%d]</A>\n",
					target, target, rest, link, link);
		} else {
			fprintf(ofp,
					"%s<A HREF=#s2i%d>[%d]</A>\n",
					rest, link, link);
		}
	}
	return 0; /* ok */
}

static int
gprof_out_filter_section_2(FILE* ifp, FILE* ofp)
{
	char buf[512];
	char* p;
	unsigned link;

	while (fgets(buf, sizeof(buf), ifp) != NULL) {
	

		if (buf[0] == '\f')
			break; /* done for this section */

		if ((p = strchr(buf, '[')) == NULL) { /* data line */
			fputs(buf, ofp); /* white space, etc */
			continue;
		}

		link = atoi(p+1);
		*p = '\0';
		
		fprintf(ofp,
				"<A NAME=s2i%d></A>%s<A HREF=#s1i%d>[%d]</A>\n",
				link, buf, link, link);
	}
	return 0; /* ok */
}

int
gprof_out_filter(FILE* ifp, FILE* ofp, unsigned do_reverse)
{
	/* only needed if we want to reverse the output order */
	FILE* tfp;

	if (do_reverse) {
		if ((tfp = tmpfile()) == NULL) {
			fprintf(stderr, "gprof filter: could not create tmp file\n");
			return -1;
		}
	} else {
		tfp = ofp;
	}

	fprintf(ofp, "<HTML>\n<BODY>\n<PRE>\n");

	if (gprof_out_filter_section_1(ifp, tfp) == -1)
		return -1;
	
	if (gprof_out_filter_section_2(ifp, ofp) == -1)
		return -1;

	if (tfp != ofp) {
		int c;

		rewind(tfp);

		while ((c = getc(tfp)) != -1)
			putc(c, ofp);

		fclose(tfp);
	}

	fprintf(ofp, "</PRE>\n</BODY>\n</HTML>\n");

	return 0;
}

extern const char *fe_progname_long; /* defined in xfe.h */

int
gmon_gprof_to(char* gmon_out, char* gprof_out, unsigned nfuncs)
{
	char* argv[16];
	int   argc = 0;
    int   child_pid;
	int status;
	char buf[16];
	FILE* fp = NULL;
	int rv = 0;
	int s;
	struct sigaction newact;
	struct sigaction oldact;

    s = _pr_intsOff; /* stop annoying assert from nspr */
    _pr_intsOff = 0;

	if ((fp = fopen(gprof_out, "w+")) == NULL) {
		fprintf(stderr, "gmon_gprof_to(): could not create %s: ", gprof_out);
		perror(NULL);
		rv = -1;
		goto return_point;
	}

	/*
	 *    gprof -b [-n topn] progname gmon_filename > tmpfile
	 */
	argv[argc++] = "gprof";
	argv[argc++] = "-b";
	argv[argc++] = "-E";
	argv[argc++] = "mcount";
	argv[argc++] = "-E";
	argv[argc++] = "internal_mcount";
	if (nfuncs != 0) {
		sprintf(buf, "%d", nfuncs);
		argv[argc++] = "-n";
		argv[argc++] = buf;
	}
	argv[argc++] = (char*)fe_progname_long;
	argv[argc++] = gmon_out;
	argv[argc] = NULL;

	/* Setup signals so that SIGCHLD is ignored as we want to do waitpid */
	newact.sa_handler = SIG_DFL;
	newact.sa_flags = 0;
	sigfillset(&newact.sa_mask);
	sigaction(SIGCHLD, &newact, &oldact);

    if ((child_pid = fork()) == -1) {
		fprintf(stderr, "gmon_dump_html_to(): could not fork()\n");
		rv = -1;
		goto return_point;
	}

	if (child_pid == 0) { /* I am the child */

		int fd = fileno(fp);

		/*
		 *    Redirect standard out.
		 */
		close(1);
		dup(fd);
		close(fd);

		/*
		 *    Close all but standard in, out, error.
		 */
		for (fd--; fd > 2; fd--)
			close(fd);

		printf("doing exec now dude!\n");

		if (execvp(argv[0], argv) == -1) {
			fprintf(stderr, "could not exec %s: ", argv[0]);
			perror(NULL);
			exit(3);
		}
		/*NOTREACHED*/
	}

	/*
	 *    wait
	 */
	if (waitpid(child_pid, &status, 0) == -1) {
		fprintf(stderr, "wait on %s failed: ", argv[0]);
		perror(NULL);
		rv = -1;
	}

return_point:

	/* Reset SIGCHLD signal hander before returning */
	sigaction(SIGCHLD, &oldact, NULL);

	if (fp != NULL)
		fclose(fp);

    _pr_intsOff = s;

	return rv;
}

static unsigned gmon_report_reverse = 1; /* flat then graph */
static unsigned gmon_report_size = 0;

unsigned
gmon_is_running()
{
	return (gmon_running != 0); /* maybe should check profiling */
}

unsigned
gmon_report_get_reverse()
{
	return gmon_report_reverse;
}

void
gmon_report_set_reverse(unsigned yep)
{
	gmon_report_reverse = yep;
}

unsigned
gmon_report_get_size()
{
	return gmon_report_size;
}

void
gmon_report_set_size(unsigned yep)
{
	gmon_report_size = yep;
}

void
gmon_reset()
{
	gmon_change_state(0); /* maybe we don't need to do this, but ahhh */

	memset(sbuf, 0, ssiz);
	memset(froms, 0, (s_textsize / HASHFRACTION));
	memset(tos, 0, (tolimit * sizeof(struct tostruct)));

    ((struct phdr *)sbuf)->lpc = s_lowpc;
    ((struct phdr *)sbuf)->hpc = s_highpc;
    ((struct phdr *)sbuf)->ncnt = ssiz;
}

int
gmon_html_filter_to(char* gprofout, char* html, unsigned reverse)
{
	FILE* ifp;
	FILE* ofp;
	int rv = 0;

	if ((ifp = fopen(gprofout, "r")) == NULL) {
		fprintf(stderr, "gmon_html_filter_to(): could not open %s", gprofout);
		perror(NULL);
		return -1;
	}

	if ((ofp = fopen(html, "w")) == NULL) {
		fprintf(stderr, "gmon_html_filter_to(): could not create %s", html);
		perror(NULL);
		fclose(ifp);
		return -1;
	}

	if (gprof_out_filter(ifp, ofp, reverse) == -1)
		rv = -1;
	
	fclose(ifp);
	fclose(ofp);

	return rv;
}

