/*
** GIFtrans v1.11.1
**
** Convert any GIF file into a GIF89a
** Allows for setting the transparent or background color, changing colors,
** adding or removing comments. Also code to analyze GIF contents.
**
** Copyright (c) 24.2.94 by Andreas Ley <ley@rz.uni-karlsruhe.de>
**
** Permission to use, copy, modify, and distribute this software for any
** purpose and without fee is hereby granted, provided that the above
** copyright notice appears in all copies. This software is provided "as is"
** and without any express or implied warranties.
**
** This program has been tested on a HP9000/720 with HP-UX A.08.07
** In this environment, neither lint -u nor gcc -Wall produce any messages.
** If you encounter any errors or need to make any changes to port it
** to another platform, please contact me.
**
** Known bugs:
**	-B flag won't work if there's an Extension between the Global Color
**	Table and the Image Descriptor (or Graphic Control Extension). If -V
**	has been specified, a Warning Message will be displayed.
**	Will be fixed in 2.0
**	Always outputs GIF89a. Shouldn't do this if version is newer.
**	-D option may output changed data instead of original data, use
**	with caution, best only with then -L option.
**
** Version history
**
** Version 1.11.1 - 11.8.94
**	Allows for use of the -g option without the -B option.
**
** Version 1.11 - 21.7.94
**	Moved Plain Text Extension to the Extensions section where it belongs.
**	Accept Unknown Extension Labels.
**	Incorporated MS-DOS port by enzo@hk.net (Enzo Michelangeli).
**	Added -o and -e options to redirect stdout and stderr.
**	Added -D debug flag.
**
** Version 1.10.2 - 22.6.94
**	Support for -DRGBTXT flag.
**
** Version 1.10.1 - 21.6.94
**	Different rgb.txt file FreeBSD/386BSD.
**
** Version 1.10 - 19.6.94
**	Added -g option to change a color in the global color table.
**	Added -B option to change the color for the transparent color index.
**
** Version 1.9.1 - 7.6.94
**	Different rgb.txt files for X11 and Open Windows.
**
** Version 1.9 - 1.6.94
**	Fixed a bug which caused color names to be rejected.
**
** Version 1.8 - 30.5.94
**	Accept #rrggbb style arguments.
**	Do nothing if rgb-color not found in GIF.
**
** Version 1.7 - 16.5.94
**	Added -l option to only list the color table.
**	Added -L option for verbose output without creating a gif.
**	Added -b option to change the background color index.
**	Display all matching color names for color table entries.
**	Fixed a bug which caused bad color names if rgb.txt starts with
**		whitespace.
**	Doesn't use strdup anymore.
**	Fixed =& bug on dec machines.
**
** Version 1.6 - 5.4.94
**	Added color names recognition.
**
** Version 1.5 - 15.3.94
**	Added basic verbose output to analyze GIFs.
**
** Version 1.4 - 8.3.94
**	Fixed off-by-one bug in Local Color table code.
**	Added -c and -C options to add or remove a comment.
**	Transparency is no longer the default.
**
** Thanx for bug reports, ideas and fixes to
**	patricka@cs.kun.nl (Patrick Atoon)
**	wes@msc.edu (Wes Barris)
**	pmfitzge@ingr.com (Patrick M. Fitzgerald)
**	hoesel@chem.rug.nl (Frans van Hoesel)
**	boardman@jerry.sal.wisc.edu (Dan Boardman)
**	krweiss@chip.ucdavis.edu (Ken Weiss)
**	chuck.musciano@harris.com (Chuck Musciano)
**	heycke@camis.stanford.edu (Torsten Heycke)
**	claw@spacsun.rice.edu (Colin Law)
**	jwalker@eos.ncsu.edu (Joseph C. Walker)
**	Bjorn.Borud@alkymi.unit.no (Bjorn Borud)
**	Christopher.Vance@adfa.oz.au (CJS Vance)
**	pederl@norway.hp.com (Peder Langlo)
**	I.Rutson@bradford.ac.uk (Ian Rutson)
**	Nicolas.Pioch@enst.fr (Nicolas Pioch)
**	john@charles.CS.UNLV.EDU (John Kilburg)
**	enzo@hk.net (Enzo Michelangeli)
**	twv@hpwtwe0.cup.hp.com (Terry von Gease)
**
** Original distribution site is
**	ftp://ftp.rz.uni-karlsruhe.de/pub/net/www/tools/giftrans.c
** A man-page by knordlun@fltxa.helsinki.fi (Kai Nordlund) is at
**	ftp://ftp.rz.uni-karlsruhe.de/pub/net/www/tools/giftrans.1
** To compile for MS-DOS, you need getopt:
**	ftp://ftp.rz.uni-karlsruhe.de/pub/net/www/tools/getopt.c
** MS-DOS executable can be found at
**	ftp://ftp.rz.uni-karlsruhe.de/pub/net/www/tools/giftrans.exe
** A template rgb.txt for use with the MS-DOS version can be found at
**	ftp://ftp.rz.uni-karlsruhe.de/pub/net/www/tools/rgb.txt
** Additional info can be found on
**	http://melmac.corp.harris.com/transparent_images.html
*/

#define	X11		/* When using X Window System */
#undef	OPENWIN		/* When using Open Windows */
#undef	X386		/* When using FreeBSD/386BSD */
#undef	MSDOS		/* When using Borland C (maybe MSC too) */

char header[]="GIFtrans v1.11.1\n(c) 1994 by Andreas Ley\n";

#ifndef RGBTXT
#ifdef X11
#define	RGBTXT	"/usr/lib/X11/rgb.txt"
#else /* X11 */
#ifdef OPENWIN
#define	RGBTXT	"/usr/openwin/lib/rgb.txt"
#else /* OPENWIN */
#ifdef X386
#define	RGBTXT	"/usr/X386/lib/X11/rgb.txt"
#else /* X386 */
#ifdef MSDOS
#define	RGBTXT	"rgb.txt"
#else /* MSDOS */
#define	RGBTXT	"";
#endif /* MSDOS */
#endif /* X386 */
#endif /* OPENWIN */
#endif /* X11 */
#endif /* RGBTXT */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef MSDOS
#include <unistd.h>
#include <sys/param.h>
#else
#include <fcntl.h>
#include "getopt.c"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif /* MAXPATHLEN */

#define	FALSE	(0)		/* This is the naked Truth */
#define	TRUE	(1)		/* and this is the Light */

#define	SUCCESS	(0)
#define	FAILURE	(1)

struct entry {
	struct entry	*next;
	char		*name;
	int		red;
	int		green;
	int		blue;
	} *root;

#define	NONE	(-1)
#define	OTHER	(-2)
#define	RGB	(-3)

struct color {
	int		index;
	int		red;
	int		green;
	int		blue;
	} bc,tc,tn,go,gn;

static char	*image,*comment;
static int	skipcomment,list,verbose,output,debug;
static long int	pos;

static char	rgb[] = RGBTXT;
static char	true[] = "True";
static char	false[] = "False";

#define	readword(buffer)	((buffer)[0]+256*(buffer)[1])
#define	readflag(buffer)	((buffer)?true:false)
#define	hex(c)			('a'<=(c)&&(c)<='z'?(c)-'a'+10:'A'<=(c)&&(c)<='Z'?(c)-'A'+10:(c)-'0')


void dump(adr,data,len)
long int	adr;
unsigned char	*data;
size_t		len;
{
	int	i;

	while (len>0) {
		(void)fprintf(stderr,"%08lx:%*s",adr,(int)((adr%16)*3+(adr%16>8?1:0)),"");
		for (i=adr%16;i<16&&len>0;i++,adr++,data++,len--)
			(void)fprintf(stderr,"%s%02x",i==8?"  ":" ",*data);
		(void)fprintf(stderr,"\n");
	}
}



void writedata(dest,data,len)
FILE		*dest;
unsigned char	*data;
size_t		len;
{
	unsigned char	size;

	while (len) {
		size=len<256?len:255;
		(void)fwrite((void *)&size,1,1,dest);
		(void)fwrite((void *)data,(size_t)size,1,dest);
		data+=size;
		len-=size;
	}
	size=0;
	(void)fwrite((void *)&size,1,1,dest);
}


void skipdata(src)
FILE	*src;
{
	unsigned char	size,buffer[256];

	do {
		pos=ftell(src);
		(void)fread((void *)&size,1,1,src);
		if (debug)
			dump(pos,&size,1);
		if (debug) {
			pos=ftell(src);
			(void)fread((void *)buffer,(size_t)size,1,src);
			dump(pos,buffer,(size_t)size);
		}
		else
			(void)fseek(src,(long int)size,SEEK_CUR);
	} while (!feof(src)&&size>0);
}


void transblock(src,dest)
FILE	*src;
FILE	*dest;
{
	unsigned char	size,buffer[256];

	pos=ftell(src);
	(void)fread((void *)&size,1,1,src);
	if (debug)
		dump(pos,&size,1);
	if (output)
		(void)fwrite((void *)&size,1,1,dest);
	pos=ftell(src);
	(void)fread((void *)buffer,(size_t)size,1,src);
	if (debug)
		dump(pos,buffer,(size_t)size);
	if (output)
		(void)fwrite((void *)buffer,(size_t)size,1,dest);
}


void transdata(src,dest)
FILE	*src;
FILE	*dest;
{
	unsigned char	size,buffer[256];

	do {
		pos=ftell(src);
		(void)fread((void *)&size,1,1,src);
		if (debug)
			dump(pos,&size,1);
		if (output)
			(void)fwrite((void *)&size,1,1,dest);
		pos=ftell(src);
		(void)fread((void *)buffer,(size_t)size,1,src);
		if (debug)
			dump(pos,buffer,(size_t)size);
		if (output)
			(void)fwrite((void *)buffer,(size_t)size,1,dest);
	} while (!feof(src)&&size>0);
}


int giftrans(src,dest)
FILE	*src;
FILE	*dest;
{
	unsigned char	buffer[3*256],lsd[7],gct[3*256],gce[5];
	unsigned int	cnt,cols,size,gct_size,gct_delay,gce_present;
	struct entry	*rgbptr;


	/* Header */
	pos=ftell(src);
	(void)fread((void *)buffer,6,1,src);
	if (strncmp((char *)buffer,"GIF",3)) {
		(void)fprintf(stderr,"No GIF file!\n");
		return(1);
	}
	if (verbose) {
		buffer[6]='\0';
		(void)fprintf(stderr,"Header: \"%s\"\n",buffer);
	}
	if (debug)
		dump(pos,buffer,6);
	if (output)
		(void)fputs("GIF89a",dest);

	/* Logical Screen Descriptor */
	pos=ftell(src);
	(void)fread((void *)lsd,7,1,src);
	if (verbose) {
		(void)fprintf(stderr,"Logical Screen Descriptor:\n");
		(void)fprintf(stderr,"\tLogical Screen Width: %d pixels\n",readword(lsd));
		(void)fprintf(stderr,"\tLogical Screen Height: %d pixels\n",readword(lsd+2));
		(void)fprintf(stderr,"\tGlobal Color Table Flag: %s\n",readflag(lsd[4]&0x80));
		(void)fprintf(stderr,"\tColor Resolution: %d bits\n",(lsd[4]&0x70>>4)+1);
		if (lsd[4]&0x80) {
			(void)fprintf(stderr,"\tSort Flag: %s\n",readflag(lsd[4]&0x8));
			(void)fprintf(stderr,"\tSize of Global Color Table: %d colors\n",2<<(lsd[4]&0x7));
			(void)fprintf(stderr,"\tBackground Color Index: %d\n",lsd[5]);
		}
		if (lsd[6])
			(void)fprintf(stderr,"\tPixel Aspect Ratio: %d (Aspect Ratio %f)\n",lsd[6],((double)lsd[6]+15)/64);
	}
	if (debug)
		dump(pos,lsd,7);

	/* Global Color Table */
	gct_delay=FALSE;
	if (lsd[4]&0x80) {
		gct_size=2<<(lsd[4]&0x7);
		pos=ftell(src);
		(void)fread((void *)gct,gct_size,3,src);
		if (go.index==RGB)
			for(cnt=0;cnt<gct_size&&go.index==RGB;cnt++)
				if (gct[3*cnt]==go.red&&gct[3*cnt+1]==go.green&&gct[3*cnt+2]==go.blue)
					go.index=cnt;
		if (go.index>=0) {
			if (gn.index>=0) {
				gn.red=gct[3*gn.index];
				gn.green=gct[3*gn.index+1];
				gn.blue=gct[3*gn.index+2];
			}
			gct[3*go.index]=gn.red;
			gct[3*go.index+1]=gn.green;
			gct[3*go.index+2]=gn.blue;
		}
		if (bc.index==RGB)
			for(cnt=0;cnt<gct_size&&bc.index==RGB;cnt++)
				if (gct[3*cnt]==bc.red&&gct[3*cnt+1]==bc.green&&gct[3*cnt+2]==bc.blue)
					bc.index=cnt;
		if (bc.index>=0)
			lsd[5]=bc.index;
		if (tc.index==RGB)
			for(cnt=0;cnt<gct_size&&tc.index==RGB;cnt++)
				if (gct[3*cnt]==tc.red&&gct[3*cnt+1]==tc.green&&gct[3*cnt+2]==tc.blue)
					tc.index=cnt;
		if (tc.index==OTHER)
			tc.index=lsd[5];
		if (tn.index>=0) {
			tn.red=gct[3*tn.index];
			tn.green=gct[3*tn.index+1];
			tn.blue=gct[3*tn.index+2];
		}
		if (tn.index!=NONE)
			gct_delay=TRUE;
	}
	if (output)
		(void)fwrite((void *)lsd,7,1,dest);
	if (lsd[4]&0x80) {
		if (list||verbose) {
			(void)fprintf(stderr,"Global Color Table:\n");
			for(cnt=0;cnt<gct_size;cnt++) {
				(void)fprintf(stderr,"\tColor %d: Red %d, Green %d, Blue %d",cnt,gct[3*cnt],gct[3*cnt+1],gct[3*cnt+2]);
				(void)fprintf(stderr,", #%02x%02x%02x",gct[3*cnt],gct[3*cnt+1],gct[3*cnt+2]);
				for (rgbptr=root,cols=0;rgbptr;rgbptr=rgbptr->next)
					if (rgbptr->red==gct[3*cnt]&&rgbptr->green==gct[3*cnt+1]&&rgbptr->blue==gct[3*cnt+2])
						(void)fprintf(stderr,"%s%s",cols++?", ":" (",rgbptr->name);
				(void)fprintf(stderr,"%s\n",cols?")":"");
			}
		}
		if (debug)
			dump(pos,gct,gct_size*3);
		if (output&&(!gct_delay))
			(void)fwrite((void *)gct,gct_size,3,dest);
	}

	gce_present=FALSE;
	do {
		pos=ftell(src);
		(void)fread((void *)buffer,1,1,src);
		switch (buffer[0]) {
		case 0x2c:	/* Image Descriptor */
			if (verbose)
				(void)fprintf(stderr,"Image Descriptor:\n");
			(void)fread((void *)(buffer+1),9,1,src);
			/* Write Graphic Control Extension */
			if (tc.index>=0||gce_present) {
				if (!gce_present) {
					gce[0]=0;
					gce[1]=0;
					gce[2]=0;
				}
				if (tc.index>=0) {
					gce[0]|=0x01;	/* Set Transparent Color Flag */
					gce[3]=tc.index;	/* Set Transparent Color Index */
				}
				else if (gce[0]&0x01)
					tc.index=gce[3];	/* Remember Transparent Color Index */
				gce[4]=0;
				if (tc.index>=0&&(!(buffer[8]&0x80))) { /* Transparent Color Flag set and no Local Color Table */
					gct[3*tc.index]=tn.red;
					gct[3*tc.index+1]=tn.green;
					gct[3*tc.index+2]=tn.blue;
				}
				if (output&&gct_delay) {
					(void)fwrite((void *)gct,gct_size,3,dest);
					gct_delay=FALSE;
				}
				if (output) {
					(void)fputs("\041\371\004",dest);
					(void)fwrite((void *)gce,5,1,dest);
				}
			}
			if (output&&gct_delay) {
				if (verbose)
					(void)fprintf(stderr,"Warning: Global Color Table has not been modified as no Transparent Color Index has been set\n");
				(void)fwrite((void *)gct,gct_size,3,dest);
				gct_delay=FALSE;
			}
			/* Write Image Descriptor */
			if (verbose) {
				(void)fprintf(stderr,"\tImage Left Position: %d pixels\n",readword(buffer+1));
				(void)fprintf(stderr,"\tImage Top Position: %d pixels\n",readword(buffer+3));
				(void)fprintf(stderr,"\tImage Width: %d pixels\n",readword(buffer+5));
				(void)fprintf(stderr,"\tImage Height: %d pixels\n",readword(buffer+7));
				(void)fprintf(stderr,"\tLocal Color Table Flag: %s\n",readflag(buffer[9]&0x80));
				(void)fprintf(stderr,"\tInterlace Flag: %s\n",readflag(buffer[9]&0x40));
				if (buffer[9]&0x80) {
					(void)fprintf(stderr,"\tSort Flag: %s\n",readflag(buffer[9]&0x20));
					(void)fprintf(stderr,"\tSize of Global Color Table: %d colors\n",2<<(buffer[9]&0x7));
				}
			}
			if (debug)
				dump(pos,buffer,10);
			if (output)
				(void)fwrite((void *)buffer,10,1,dest);
			/* Local Color Table */
			if (buffer[8]&0x80) {
				size=2<<(buffer[8]&0x7);
				pos=ftell(src);
				(void)fread((void *)buffer,size,3,src);
				if (verbose) {
					(void)fprintf(stderr,"Local Color Table:\n");
					for(cnt=0;cnt<size;cnt++)
						(void)fprintf(stderr,"\tColor %d: Red %d, Green %d, Blue %d\n",cnt,buffer[3*cnt],buffer[3*cnt+1],buffer[3*cnt+2]);
				}
				if (tc.index>=0) { /* Transparent Color Flag set */
					buffer[3*tc.index]=tn.red;
					buffer[3*tc.index+1]=tn.green;
					buffer[3*tc.index+2]=tn.blue;
				}
				if (debug)
					dump(pos,buffer,size*3);
				if (output)
					(void)fwrite((void *)buffer,size,3,dest);
			}
			/* Table Based Image Data */
			pos=ftell(src);
			(void)fread((void *)buffer,1,1,src);
			if (verbose) {
				(void)fprintf(stderr,"Table Based Image Data:\n");
				(void)fprintf(stderr,"\tLZW Minimum Code Size: 0x%02x\n",buffer[0]);
			}
			if (debug)
				dump(pos,buffer,1);
			if (output)
				(void)fwrite((void *)buffer,1,1,dest);
			transdata(src,dest);
			gce_present=FALSE;
			break;
		case 0x3b:	/* Trailer */
			if (verbose)
				(void)fprintf(stderr,"Trailer\n");
			if (debug)
				dump(pos,buffer,1);
			if (comment&&*comment&&output) {
				(void)fputs("\041\376",dest);
				writedata(dest,(unsigned char *)comment,strlen(comment));
			}
			if (output)
				(void)fwrite((void *)buffer,1,1,dest);
			break;
		case 0x21:	/* Extension */
			(void)fread((void *)(buffer+1),1,1,src);
			switch (buffer[1]) {
			case 0x01:	/* Plain Text Extension */
				if (output&&gct_delay) {
					if (verbose)
						(void)fprintf(stderr,"Warning: Global Color Table has not been modified due to a Plain Text Extension\n");
					(void)fwrite((void *)gct,gct_size,3,dest);
					gct_delay=FALSE;
				}
				if (verbose)
					(void)fprintf(stderr,"Plain Text Extension\n");
				if (debug)
					dump(pos,buffer,2);
				if (output)
					(void)fwrite((void *)buffer,2,1,dest);
				transblock(src,dest);
				transdata(src,dest);
				break;
			case 0xf9:	/* Graphic Control Extension */
				if (verbose)
					(void)fprintf(stderr,"Graphic Control Extension:\n");
				(void)fread((void *)(buffer+2),1,1,src);
				size=buffer[2];
				(void)fread((void *)gce,size,1,src);
				if (verbose) {
					(void)fprintf(stderr,"\tDisposal Method: %d ",gce[0]&0x1c>>2);
					switch (gce[0]&0x1c>>2) {
					case 0:
						(void)fprintf(stderr,"(no disposal specified)\n");
						break;
					case 1:
						(void)fprintf(stderr,"(do not dispose)\n");
						break;
					case 2:
						(void)fprintf(stderr,"(restore to background color)\n");
						break;
					case 3:
						(void)fprintf(stderr,"(restore to previous)\n");
						break;
					default:
						(void)fprintf(stderr,"(to be defined)\n");
					}
					(void)fprintf(stderr,"\tUser Input Flag: %s\n",readflag(gce[0]&0x2));
					(void)fprintf(stderr,"\tTransparent Color Flag: %s\n",readflag(gce[0]&0x1));
					(void)fprintf(stderr,"\tDelay Time: %d\n",readword(gce+1));
					if (gce[0]&0x1)
						(void)fprintf(stderr,"\tTransparent Color Index: %d\n",gce[3]);
				}
				if (debug) {
					dump(pos,buffer,3);
					dump(pos+3,gce,size);
				}
				pos=ftell(src);
				(void)fread((void *)buffer,1,1,src);
				if (debug)
					dump(pos,buffer,1);
				gce_present=TRUE;
				break;
			case 0xfe:	/* Comment Extension */
				if (verbose)
					(void)fprintf(stderr,"Comment Extension\n");
				if (debug)
					dump(pos,buffer,2);
				if (skipcomment)
					skipdata(src);
				else {
					if (output&&gct_delay) {
						if (verbose)
							(void)fprintf(stderr,"Warning: Global Color Table has not been modified due to a Comment Extension\n");
						(void)fwrite((void *)gct,gct_size,3,dest);
						gct_delay=FALSE;
					}
					if (output)
						(void)fwrite((void *)buffer,2,1,dest);
					transdata(src,dest);
				}
				break;
			case 0xff:	/* Application Extension */
				if (output&&gct_delay) {
					if (verbose)
						(void)fprintf(stderr,"Warning: Global Color Table has not been modified due to a Application Extension\n");
					(void)fwrite((void *)gct,gct_size,3,dest);
					gct_delay=FALSE;
				}
				if (verbose)
					(void)fprintf(stderr,"Application Extension\n");
				if (debug)
					dump(pos,buffer,2);
				if (output)
					(void)fwrite((void *)buffer,2,1,dest);
				transblock(src,dest);
				transdata(src,dest);
				break;
			default:
				if (output&&gct_delay) {
					if (verbose)
						(void)fprintf(stderr,"Warning: Global Color Table has not been modified due to an unknown Extension\n");
					(void)fwrite((void *)gct,gct_size,3,dest);
					gct_delay=FALSE;
				}
				if (verbose)
					(void)fprintf(stderr,"Unknown label: 0x%02x\n",buffer[1]);
				if (debug)
					dump(pos,buffer,2);
				if (output)
					(void)fwrite((void *)buffer,2,1,dest);
				transblock(src,dest);
				transdata(src,dest);
				break;
			}
			break;
		default:
			(void)fprintf(stderr,"0x%08lx: Unknown extension 0x%02x!\n",ftell(src)-1,buffer[0]);
			if (debug)
				dump(pos,buffer,1);
			return(1);
		}
	} while (buffer[0]!=0x3b&&!feof(src));
	return(buffer[0]==0x3b?SUCCESS:FAILURE);
}



int getindex(c,arg)
struct color	*c;
char	*arg;
{
	struct entry	*ptr;

	if ('0'<=*arg&&*arg<='9')
		c->index=atoi(arg);
	else if (*arg=='#') {
		if (strlen(arg)==4) {
			c->index=RGB;
			c->red=hex(arg[1])<<4;
			c->green=hex(arg[2])<<4;
			c->blue=hex(arg[3])<<4;
		}
		else if (strlen(arg)==7) {
			c->index=RGB;
			c->red=(hex(arg[1])<<4)+hex(arg[2]);
			c->green=(hex(arg[3])<<4)+hex(arg[4]);
			c->blue=(hex(arg[5])<<4)+hex(arg[6]);
		}
		else {
			(void)fprintf(stderr,"%s: illegal color specification: %s\n",image,arg);
			return(FAILURE);
		}
	}
	else {
		for (ptr=root;ptr&&c->index!=RGB;ptr=ptr->next)
			if (!strcmp(ptr->name,arg)) {
				c->index=RGB;
				c->red=ptr->red;
				c->green=ptr->green;
				c->blue=ptr->blue;
			}
		if (c->index!=RGB) {
			(void)fprintf(stderr,"%s: no such color: %s\n",image,arg);
			return(FAILURE);
		}
	}
	return(SUCCESS);
}



void usage()
{
	(void)fprintf(stderr,"Usage: %s [-t color|-T] [-B color] [-b color] [-g oldcolor=newcolor] [-c comment|-C] [-l|-L|-V] [-o filename] [-e filename] [filename]\n",image);
	(void)fprintf(stderr,"Convert any GIF file into a GIF89a, with the folloing changes possible:\n");
	(void)fprintf(stderr,"-t Specify the transparent color\n");
	(void)fprintf(stderr,"-T Index of the transparent color is the background color index\n");
	(void)fprintf(stderr,"-B Specify the transparent color's new value\n");
	(void)fprintf(stderr,"-b Specify the background color\n");
	(void)fprintf(stderr,"-g Change a color in the global color table\n");
	(void)fprintf(stderr,"-c Add a comment\n");
	(void)fprintf(stderr,"-C Remove old comment\n");
	(void)fprintf(stderr,"-l Only list the color table\n");
	(void)fprintf(stderr,"-L Verbose output of GIFs contents\n");
	(void)fprintf(stderr,"-V Verbose output while converting\n");
	(void)fprintf(stderr,"-o Redirect stdout to a file\n");
	(void)fprintf(stderr,"-e Redirect stderr to a file\n");
	if (*rgb)
		(void)fprintf(stderr,"Colors may be specified as index, as rgb.txt entry or in the #rrggbb form.\n");
	else
		(void)fprintf(stderr,"Colors may be specified as index or in the #rrggbb form.\n");
	exit(1);
}


int main(argc,argv)
int	argc;
char	*argv[];
{
	int		c;
	extern char	*optarg;
	extern int	optind;
	char		error[2*MAXPATHLEN+14],line[BUFSIZ],*ptr,*nptr,*oname,*ename;
	struct entry	**next;
	FILE		*src;
	int		stat;

	image=argv[0];
	root=NULL;
	if (*rgb)
		if ((src=fopen(rgb,"r"))!=NULL) {
			next= &root;
			while (fgets(line,sizeof(line),src)) {
				*next=(struct entry *)malloc(sizeof(struct entry));
				for (ptr=line;strchr(" \t",*ptr);ptr++);
				for (nptr=ptr;!strchr(" \t",*ptr);ptr++);
				*ptr++='\0';
				(*next)->red=atoi(nptr);
				for (;strchr(" \t",*ptr);ptr++);
				for (nptr=ptr;!strchr(" \t",*ptr);ptr++);
				*ptr++='\0';
				(*next)->green=atoi(nptr);
				for (;strchr(" \t",*ptr);ptr++);
				for (nptr=ptr;!strchr(" \t",*ptr);ptr++);
				*ptr++='\0';
				(*next)->blue=atoi(nptr);
				for (;strchr(" \t",*ptr);ptr++);
				for (nptr=ptr;!strchr(" \t\r\n",*ptr);ptr++);
				*ptr='\0';
				(void)strcpy((*next)->name=(char *)malloc(strlen(nptr)+1),nptr);
				(*next)->next=NULL;
				next= &(*next)->next;
			}
			(void)fclose(src);
		}
		else {
#ifndef MSDOS
			(void)sprintf(error,"%s: cannot open %s",image,rgb);
			perror(error);
			return(FAILURE);
#else /* MSDOS */
			*rgb='\0';
#endif
		}

	bc.index=NONE;
	tc.index=NONE;
	tn.index=NONE;
	go.index=NONE;
	gn.index=NONE;
	comment=NULL;
	skipcomment=FALSE;
	verbose=FALSE;
	output=TRUE;
	debug=FALSE;
	oname=NULL;
	ename=NULL;
	while ((c=getopt(argc,argv,"t:TB:b:g:c:ClLVDo:e:vh?")) != EOF)
		switch ((char)c) {
		case 'b':
			if (getindex(&bc,optarg))
				return(FAILURE);
			break;
		case 't':
			if (getindex(&tc,optarg))
				return(FAILURE);
			break;
		case 'T':
			tc.index=OTHER;
			break;
		case 'B':
			if (getindex(&tn,optarg))
				return(FAILURE);
			break;
		case 'g':
			if ((ptr=strchr(optarg,'='))!=NULL) {
				*ptr++='\0';
				if (getindex(&go,optarg))
					return(FAILURE);
				if (getindex(&gn,ptr))
					return(FAILURE);
			}
			else
				usage();
			break;
		case 'c':
			comment=optarg;
			break;
		case 'C':
			skipcomment=TRUE;
			break;
		case 'l':
			list=TRUE;
			output=FALSE;
			break;
		case 'L':
			verbose=TRUE;
			output=FALSE;
			break;
		case 'V':
			verbose=TRUE;
			break;
		case 'D':
			debug=TRUE;
			break;
		case 'o':
			oname=optarg;
			break;
		case 'e':
			ename=optarg;
			break;
		case 'v':
			(void)fprintf(stderr,header);
			return(0);
		case 'h':
			(void)fprintf(stderr,header);
		case '?':
			usage();
		}
	if (optind+1<argc||(bc.index==NONE&&tc.index==NONE&&tn.index==NONE&&gn.index==NONE&&comment==NULL&&!skipcomment&&!list&&!verbose))
		usage();

	if (oname&&freopen(oname,"wb",stdout)==NULL) {
		(void)sprintf(error,"%s: cannot open %s",image,oname);
		perror(error);
		return(FAILURE);
	}

	if (ename&&freopen(ename,"wb",stderr)==NULL) {
		(void)sprintf(error,"%s: cannot open %s",image,ename);
		perror(error);
		return(FAILURE);
	}

#ifdef MSDOS
	if(oname==NULL&&(stdout->flags&_F_TERM)==0&&setmode(fileno(stdout),O_BINARY)!=0) {
		(void)fprintf(stderr,"%s: can't set stdout's mode to binary\n",image);
		exit(2);
	}
	if(optind==argc&&(stdin->flags&_F_TERM)==0&&setmode(fileno(stdin),O_BINARY)) {
		(void)fprintf(stderr,"%s: can't set stdin's mode to binary\n",image);
		exit(2);
	}
#endif /* MSDOS */

	if (optind<argc)
		if (strcmp(argv[optind],"-"))
			if ((src=fopen(argv[optind],"rb"))!=NULL) {
				stat=giftrans(src,stdout);
				(void)fclose(src);
			}
			else {
				(void)sprintf(error,"%s: cannot open %s",image,argv[optind]);
				perror(error);
				return(FAILURE);
			}
		else
			stat=giftrans(stdin,stdout);
	else
		stat=giftrans(stdin,stdout);

	(void)fclose(stdout);
	(void)fclose(stderr);
	return(stat);
}
