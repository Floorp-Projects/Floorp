/*
The contents of this file are subject to the Mozilla Public License
Version 1.1 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is Eduardo Trapani.
Portions created by are Eduardo Trapani Copyright (C) 2000
Eduardo Trapani. All Rights Reserved.

Contributor(s):

Alternatively, the contents of this file may be used under the terms
of the GNU General Public License (the "GPL"), in which case the
provisions of the GPL are applicable instead of those above.  If you
wish to allow use of your version of this file only under the terms of
the GPL and not to allow others to use your version of this file under
the MPL, indicate your decision by deleting the provisions above and
replace them with the notice and other provisions required by the
GPL. If you do not delete the provisions above, a recipient may use
your version of this file under either the MPL or the GPL.
*/

/*
	This programs reads the dtd files in the Mozilla chrome subdirectories
	and parses them so that they can be included in an SQL database.
	I guess it could have been written in Perl, but I am not fluent with
	it so I went for "C".

	The output can be sent directly to the SQL engine.

	The idea is to have two tables:

	original:  package, file, entity, original_text, localization_notes
	translation:  package, locale, file, entity, email, translated_text

	This allows to have all the proposal in a file (email is part of the
	key as are package, locale, file and entity).  Another table may be
	used to select which translations makes it to the chrome if there is
	more than one for the same package/locale/file/entity.

	To run type:

	dtd2mysql package locale path_to_chrome

	package is navigator, messenger, etc.
	locale is en-US, eo, de-AT, etc.
	path_to_chrome is, for example /tmp/package/chrome if you unpacked
	the build under /tmp.

	IMPORTANT!  You have to change the code and recompile to change the
	output for the "original" or "translation" tables.  Just change
	the call to print_query so that it has ORIGINAL or PROPOSAL as the
	first parameter.

	Please email me if you have comments, suggestions or have found
	bugs to etrapani@unesco.org.uy.

	Author:		Eduardo Trapani (etrapani@unesco.org.uy)
	Date:		March 14, 2000
	Bugs:		None (yet)

	Changes:	2000-03-21
			New parsing code
*/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#define	MAXENTITY	256
#define	MAXVALUE	8192
#define	MAXWHAT		256
#define	MAXNOTE		1024
#define	MAXTAG		MAXENTITY + MAXVALUE + MAXWHAT + MAXNOTE + 50

#define	END		1
#define	ENTITY		2
#define	SINGLE_NOTE	3
#define	NOTE_BEGIN	4
#define	NOTE_END	5
#define	COMMENT		6
#define	NONOTE		7

#define	ORIGINAL	1
#define	PROPOSAL	2

struct info
{
char	what[MAXENTITY + 1];
char	entity[MAXENTITY + 1];
char	value[MAXVALUE + 1];
char	note[MAXNOTE + 1];
int	snote;
};

int	next_tag(FILE *asa,char *buffer,int length)
{
int	i,c;

	i = 0;
	while ((c = fgetc(asa)) != EOF && c != '<')
		;

	while ((c = fgetc(asa)) != EOF && c != '>' && i < length)
		buffer[i++] = c;

	buffer[i] = 0;

	return(c);
}
void clean(struct info *inf)
{
	inf->what[0] = 0;
	inf->entity[0] = 0;
	inf->value[0] = 0;
	if (inf->snote != NOTE_BEGIN)
	{
		inf->note[0] = 0;
		inf->snote = NONOTE;
	}
}
void	print(char *prefix,struct info *inf)
{
	printf("(%s.%s)=%s[%s]\n",prefix,inf->entity,inf->value,inf->note);
	return;
}
void	print_query(int cual,char *package,char *locale,char *filename,struct info *inf)
{
	if (cual == PROPOSAL)
	{
		printf("replace into translation (email,package,locale,file,entity,translated_text,note) values(\"etrapani@unesco.org.uy\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\");\n",package,locale,filename,inf->entity,inf->value,inf->note);
	}
	else if (cual == ORIGINAL)
	{
		printf("replace into original (package,locale,file,entity,original_text,note) values(\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\");\n",package,locale,filename,inf->entity,inf->value,inf->note);
	}
	return;
}
main(int argc,char *argv[])
{
FILE	*asa;
DIR	*dir;
struct info information;
int	ret;
char	filename[256];
char	directory[256];
char	tag[MAXTAG];
struct dirent *current;

	if (argc < 4)
	{
		printf("Usage: prog package language path_to_chrome\n");
		return(0);
	}
	sprintf(directory,"%s/%s/locale/%s",argv[3],argv[1],argv[2]);
	if (chdir(directory) != 0)
	{
	char	msg[128];

		sprintf(msg,"dtd2mysql cd to %s",directory);
		perror(msg);
		return(0);
	}
	dir = opendir(".");
	if (dir == NULL)
		return(0);

	while ((current = readdir(dir)) != NULL)
	{
		sprintf(filename,"%s/%s/locale/%s/%s",argv[3],argv[1],argv[2],current->d_name);
		asa =fopen(filename,"r");
		if (asa == NULL)
		{
		char	msg[128];

			sprintf(msg,"dtd2mysql opening %s",filename);
			perror(msg);
			continue;
		}
		else
			fprintf(stderr,"Procesando %s\n",filename);
		clean(&information);
		while (next_tag(asa,tag,MAXTAG -1) != EOF)
		{
			ret = parse(tag,&information);
			if (ret == ENTITY)
			{
				print_query(ORIGINAL,argv[1],argv[2],current->d_name,&information);
				clean(&information);
			}
			else if (ret == SINGLE_NOTE)
				information.snote = SINGLE_NOTE;
			else if (ret == NOTE_BEGIN)
				information.snote = NOTE_BEGIN;
			else if (ret == NOTE_END)
			{
				information.snote = NONOTE;
				clean(&information);
			}
		}
		fclose(asa);
	}
	closedir(dir);
}
int	parse(char *tag,struct info *inf)
{
int		c,i;

	if (*tag != '!')
		return(END);

	tag++;

	while (*tag && isspace(*tag))
		tag++;

	/* is it an entity? */
	if (strncmp(tag,"ENTITY",6) == 0)
	{
	char	*begin_quote,*end_quote;

		tag += 6;

		while (*tag && isspace(*tag))
			tag++;

		if (!*tag)
			return(END);
		i = 0;
		do
		{
			inf->entity[i++] = *tag;
			tag++;
		} while (*tag && !isspace(*tag) && i < MAXENTITY);

		if (!*tag)
			return(END);


		inf->entity[i] = 0;

		begin_quote = strchr(tag,'"');
		end_quote = strrchr(tag,'"');
		if (begin_quote == NULL || end_quote == NULL)
			return(END);

		i = 0;
		begin_quote++;
		while (begin_quote < end_quote)
		{
			inf->value[i++] = *begin_quote;
			begin_quote++;
		}

		inf->value[i] = 0;

		return(ENTITY);
	}
	else if (*tag == '-')
	{
		tag++;

		if (*tag == '-')
			tag++;
		else
			return(END);

		while (*tag && isspace(*tag))
			tag++;

		if (!*tag)
			return(END);

		i = strlen("LOCALIZATION NOTE");
		if (strncmp(tag,"LOCALIZATION NOTE",i) == 0)
		{
			tag += i;
			if (strncmp(tag," BEGIN",6) == 0)
			{
				strncpy(inf->note,tag + 5,MAXNOTE);
				return(NOTE_BEGIN);
			}
			else if (strncmp(tag," END",4) == 0)
			{
				return(NOTE_END);
			}
			else if (strstr(tag," FILE ") != NULL)
			{
				return(COMMENT);
			}
			else
			{
				i = 0;

				while (*tag)
				{
					if (*tag == '"')
						inf->note[i++] = '\\';
					inf->note[i++] = *tag;
					tag++;
				}
				inf->note[i] = 0;

				return(SINGLE_NOTE);
			}
		}
		else
			return(COMMENT);
	}
}
