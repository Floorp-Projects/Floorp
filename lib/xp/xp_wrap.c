/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* *
 * 
 *
 * xp_wrap.c --- Word wrapping.
 *
 * Created: Jamie Zawinski <jwz@netscape.com>, 24-Jan-95.
 */

#include "xp.h"
#include "libi18n.h"
#include <ctype.h>

#ifndef NOT_NULL
#define NOT_NULL(x)x
#endif


#define XP_WORD_WRAP_NEW_CODE 1
#ifdef  XP_WORD_WRAP_NEW_CODE


#undef OUTPUT
#define OUTPUT(b) \
do \
{ \
	if (out - ret >= size) \
	{ \
		unsigned char *old; \
		size *= 2; \
		old = ret; \
		ret = XP_REALLOC(old, size); \
		if (!ret) \
		{ \
			XP_FREE(old); \
			return NULL; \
		} \
		out = ret + (size / 2); \
	} \
	*out++ = b; \
} while (0)

#undef OUTPUTSTR
#define OUTPUTSTR(s) \
do \
{ \
	const char *p = s; \
    while(*p) {OUTPUT(*p); p++;} \
} while (0)
 

#undef OUTPUT_MACHINE_NEW_LINE
#if defined(XP_WIN) || defined(XP_OS2)
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	OUTPUT(CR); \
	OUTPUT(LF); \
} while (0)
#else
#ifdef XP_MAC
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	OUTPUT(CR); \
	if (c) OUTPUT(LF); \
} while (0)
#else
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	if (c) OUTPUT(CR); \
	OUTPUT(LF); \
} while (0)
#endif
#endif


#undef OUTPUT_NEW_LINE
#define OUTPUT_NEW_LINE(c) \
do \
{ \
	OUTPUT_MACHINE_NEW_LINE(c); \
	beginningOfLine = in; \
	currentColumn = 0; \
	lastBreakablePos = NULL; \
} while (0)


#undef NEW_LINE
#define NEW_LINE(c) \
do \
{ \
	if ((*in == CR) && (*(in + 1) == LF)) \
	{ \
		in += 2; \
	} \
	else \
	{ \
		in++; \
	} \
	OUTPUT_NEW_LINE(c); \
} while (0)


static unsigned char *
xp_word_wrap(int charset, unsigned char *str, int maxColumn, int checkQuoting,
			const char *prefix, int addCRLF)
{
	unsigned char	*beginningOfLine;
	int		byteWidth;
	int		columnWidth;
	int		currentColumn;
	unsigned char	*end;
	unsigned char	*in;
	unsigned char	*lastBreakablePos;
	unsigned char	*out;
	unsigned char	*p;
	unsigned char	*ret;
	int32		size;

	if (!str)
	{
		return NULL;
	}

	size = 1;
	ret = XP_ALLOC(size);
	if (!ret)
	{
		return NULL;
	}

	in = str;
	out = ret;

	beginningOfLine = str;
	currentColumn = 0;
	lastBreakablePos = NULL;

	while (*in)
	{
		if (checkQuoting && (in == beginningOfLine) && (*in == '>'))
		{
			while (*in && (*in != CR) && (*in != LF))
			{
				OUTPUT(*in++);
			}
			if (*in)
			{
				NEW_LINE(addCRLF);
				if (prefix) OUTPUTSTR(prefix);
			}
			else
			{
				/*
				 * to prevent writing out line again after
				 * the main while loop
				 */
				beginningOfLine = in;
			}
		}
		else
		{
			if ((*in == CR) || (*in == LF))
			{
				if (in != beginningOfLine)
				{
					p = beginningOfLine;
					while (p < in)
					{
						OUTPUT(*p++);
					}
				}
				NEW_LINE(addCRLF);
				if (prefix) OUTPUTSTR(prefix);
				continue;
			}
			byteWidth = INTL_CharLen(charset, in);
			columnWidth = INTL_ColumnWidth(charset, in);
			if (currentColumn + columnWidth > (maxColumn +
				(((*in == ' ') || (*in == '\t')) ? 1 : 0)))
			{
				if (lastBreakablePos)
				{
					p = beginningOfLine;
					end = lastBreakablePos - 1;
					if ((*end != ' ') && (*end != '\t'))
					{
						end++;
					}
					while (p < end)
					{
						OUTPUT(*p++);
					}
					if (addCRLF && (*end == ' ' || *end == '\t'))
					{
						OUTPUT(*end);
					}
					in = lastBreakablePos;
					OUTPUT_NEW_LINE(addCRLF);
					if (prefix) OUTPUTSTR(prefix);
					continue;
				}
			}
			if
			(
				(
					((in[0] == ' ') || (in[0] == '\t')) &&
					((in[1] != ' ') && (in[1] != '\t'))
					) ||
				(
					(INTL_CharSetType(charset) == MULTIBYTE) &&
					(
						((!(in[0] & 0x80)) && (in[1] & 0x80)) ||
						(
							(in[0] & 0x80) &&
							(INTL_KinsokuClass((int16)charset, in) !=
							 PROHIBIT_END_OF_LINE) &&
							(!(in[byteWidth] & 0x80))
							) ||
						(
							(in[0] & 0x80) &&
							(INTL_KinsokuClass((int16)charset, in) !=
							 PROHIBIT_END_OF_LINE) &&
							(in[byteWidth] & 0x80) &&
							(INTL_KinsokuClass((int16)charset, &in[byteWidth]) !=
							 PROHIBIT_BEGIN_OF_LINE)
							)
						)
					)
				)
			{
				lastBreakablePos = in + byteWidth;
			}
			in += byteWidth;
			currentColumn += columnWidth;
		}
	}

	if (in != beginningOfLine)
	{
		p = beginningOfLine;
		while (*p)
		{
			OUTPUT(*p++);
		}
	}

	OUTPUT(0);

	return ret;
}

unsigned char *
XP_WordWrap(int charset, unsigned char *str, int maxColumn, int checkQuoting)
{
	return xp_word_wrap(charset, str, maxColumn, checkQuoting, NULL, 0);
}

unsigned char *
XP_WordWrapWithPrefix(int charset, unsigned char *str, int maxColumn,
					  int checkQuoting, const char *prefix, int addCRLF)
{
	return xp_word_wrap(charset, str, maxColumn, checkQuoting, prefix, addCRLF);
}

#else /* XP_WORD_WRAP_NEW_CODE */


/* There are any number of approaches one can take to wrapping long lines
   in a body of text, and this code can be configured to do several of them,
   mainly as a legacy to the experimentation I did while writing it.  The
   states are as follows:

   NEWLINES_ARE_SIGNIFICANT
        If this is defined, then all line breaks in the input cause line
        breaks in the output as well.  If it is not defined, then single
        newlines in the input are considered to be simple whitespace, and
        a blank line (that is, two consecutive line breaks) is needed to
        break a paragraph.

   HACK_PREFIX
        If this is defined, then lines beginning with ">" will be treated
        specially.

   NEWLINES_ARE_SIGNIFICANT && !HACK_PREFIX
        In this state, all lines which are longer than the fill column will
        be wrapped, and all other lines will be left alone.

   NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX
        In this state, all lines which are longer than the fill column will
        be wrapped unless they begin with ">" in the first column.  Lines which
        are shorter than the fill column, or lines which begin with ">", will
        be left alone.

   !NEWLINES_ARE_SIGNIFICANT && !HACK_PREFIX
        In this state, all paragraphs will be re-wrapped to the fill column,
        regardless of where the line breaks were in the input.  A blank line
        delimits a paragraph.

   !NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX
        In this state, all paragraphs will be re-wrapped to the fill column,
        regardless of where the line breaks were in the input.  A blank line
        delimits a paragraph.

        Fill prefixes on paragraphs are recognised, if they begin with ">"
        followed by any number of >'s or whitespace, and the paragraphs will
        be re-filled using that prefix.

        Paragraphs are delimited by blank lines, or by changes in the prefix
        sequence.

        NOTE: The code for this state is not yet fully debugged.

  The current plan is that, when the user generates a mail or news message, we
  will use a text editor which does display-time wrapping of lines; and then,
  when the time comes to send the message, we will wrap it to a fill column
  equal to the width of the user's window.  In this way, we will (hopefully)
  insert newlines in the text in the same places where the text editor had
  wrapped them for display: what the user saw on the screen is what the person
  at the other end will see as well.

  However, this has the drawback that no line will ever be longer than the
  fill column, even when that would be better.  A common situation is this:

  = The user cites a message; this causes the message to be inserted in their
    editor with a "> " prefix on each line.

  = If a line was 79 columns in the original text, it will now be 81 columns.

  = That means that the text editor will wrap the line: it will look something
    like this:

            > This is a body of text that was just shorter than
            the
            > fill column, and is now just longer than it.  Which
            causes
            > the text to be displayed with alternating long and
            short
            > lines, which is extremely unpleasant.

    Now, if the user resized their window to be 81 columns wide instead of 80,
    that text would look "normal" again; but there are two problems with this,
    the first and most obvious being that most people aren't going to do that,
    and the second being that that would cause the user's new text to be
    filled to 81 columns as well, which would be a bad thing.

  = So, a good approach to solving this problem would be to re-fill the cited
    text before inserting it in the text area, to ensure that no line is longer
    than 79 columns after the prefix has been added; and then, when the user
    sends the message, to wrap the lines as display did.

    This would mean applying (!NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX) to the
    cited text, and then applying (NEWLINES_ARE_SIGNIFICANT && !HACK_PREFIX)
    to the outgoing text.

  = However, I couldn't get the (!NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX) 
    state to work well enough.  Which is the reason for the existance of the
    (NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX) state.  the current plan is this:

    = The user cites a message; this causes the message to be inserted in their
      editor with a "> " prefix on each line.

    = Some lines may be wider than the window, causing the display to wrap
      them unattractively.

    = When the user sends the message, we insert newlines in the text at the
      same places where the display would have wrapped the lines, *except* 
      that we don't touch lines which begin with ">".  So, our line wrapping
      is WYSIWYG for new text, but not for cited text, which is passed through
      unaltered.

    I think this should do the right thing most of the time.  Problems:

    = This will mess up on lines/paragraphs which just happen to begin with ">"
      as their first character.  That could cause us to send extremely long
      lines.  This should be pretty rare, howeveer.

    = It is possible that the user might edit a message which *looked* like it
      had a paragraph with ">" at the beginning of each line, but that really
      didn't contain any newlines at all; the implicit line wrapping done by
      display might be all that there is.  This also seems pretty unlikely.

    = It's fairly important that the algorithm implemented by this code be the
      same as the algorithm used in redisplay by the various text editors.

  This code is pretty tough to read because of all the ifdefs; once we're
  really sure how it all should work, we should rip out the unused states,
  or live with duplicated code.
 */

#define NEWLINES_ARE_SIGNIFICANT
#define HACK_PREFIX

#undef DEBUG_WRAP

/* Oops, what was I thinking.  I wrote this thing to alter the string in
   place, forgetting that on the PC, the transformation "A B" -> "A\nB"
   actually increases the length of the string ("A\r\nB").

   So...  On Unix and Mac, this produces usable strings, but on the PC,
   lone \015 characters will need to be expanded to \015\012.
 */

#undef LINEBREAK

#if defined(XP_MAC)
# define LINEBREAK "\015"
#elif defined(XP_WIN)
# define LINEBREAK "\015"
#elif defined(XP_OS2)
# define LINEBREAK "\015"
#elif defined(XP_UNIX)
# define LINEBREAK "\012"
#endif

void
XP_WordWrap (char *string, int fill_column)
{
  char *s;
  char *input = string;
  char *output = string;
  char *word_start = input;		/* beginning of last word seen */
  char *word_end = input;		/* end of last word seen */
  int column = 0;				/* current column */
  Bool in_word = FALSE;
  Bool done = FALSE;
#ifdef NEWLINES_ARE_SIGNIFICANT
# ifdef HACK_PREFIX
  Bool cited_line_p = (*string == '>');
# endif /* !HACK_PREFIX */
#else  /* !NEWLINES_ARE_SIGNIFICANT */
  Bool white_has_newline = FALSE;
# ifdef HACK_PREFIX
  char last_prefix [20];
  char new_prefix [20];
# endif /* !HACK_PREFIX */
#endif /* !NEWLINES_ARE_SIGNIFICANT */

#ifdef DEBUG_WRAP
  int L = strlen (input) + 1;
  char *I = input;
  char *O = (char *) calloc (sizeof (char), L);
  output = O;
#endif /* DEBUG_WRAP */

  if (!string || !*string)
	return;

#if !defined(NEWLINES_ARE_SIGNIFICANT) && defined(HACK_PREFIX)
# define FIND_PREFIX() \
  do { \
	char *si = input; \
	char *so = new_prefix; \
	while (so < new_prefix + sizeof (new_prefix) && \
		   (*si == ' ' || *si == '\t' || *si == '>')) \
	  *so++ = *si++; \
    so--; \
	while (so > new_prefix && isspace (*so)) \
	  so--; \
    so++; \
	*so++ = 0; \
    if (*so) \
      input = si; \
  } while (0)
#endif /* !NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX */

#if !defined(NEWLINES_ARE_SIGNIFICANT) && defined(HACK_PREFIX)
  FIND_PREFIX ();
  XP_STRCPY (last_prefix, new_prefix);

  for (s = new_prefix; *s; s++)
	{
	  *output++ = *s;
	  column++;
	}
#endif /* !NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX */

  word_start = input;
  word_end = input;

  while (!done)
	{
	  if (in_word && (XP_IS_SPACE (*input) || !*input))
		/* If we are in a word and have reached whitespace, remember
		   this position in `input' as the end of the word. */
		{
#ifdef DEBUG_WRAP
		  printf ("WHITE: \"");
		  for (s = word_end; s < word_start; s++)
			printf ("%c", *s);
		  printf ("\"\n");

		  printf ("WORD: \"");
		  for (s = word_start; s < input; s++)
			printf ("%c", *s);
		  printf ("\"\n");
#endif /* DEBUG_WRAP */

		  if (
#if defined(NEWLINES_ARE_SIGNIFICANT) && defined(HACK_PREFIX)
			  !cited_line_p &&
#endif /* NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX */
			  (column
			   + (
#ifndef NEWLINES_ARE_SIGNIFICANT
				  white_has_newline ? (input - word_start + 1) :
#endif /* !NEWLINES_ARE_SIGNIFICANT */
				  (input - word_end))
			   > fill_column)


			  /* If the word is itself longer than a line is allowed to
				 be, then don't wrap - same behavior as if this is a
				 cited line. */
			  && ((input - word_end) < fill_column)
			  )
			{
			  for (s = LINEBREAK; *s; s++)
				*output++ = *s;
			  column = 0;
#if !defined(NEWLINES_ARE_SIGNIFICANT) && defined(HACK_PREFIX)
			  for (s = new_prefix; *s; s++)
				*output++ = *s, column++;
#endif /* NEWLINES_ARE_SIGNIFICANT && HACK_PREFIX */
#ifdef DEBUG_WRAP
			  printf ("NEWLINE\n");
#endif /* DEBUG_WRAP */
			}
		  else
			{
#ifndef NEWLINES_ARE_SIGNIFICANT
			  if (white_has_newline)
				{
				  *output++ = ' ';
				  column++;
				}
			  else
#endif /* !NEWLINES_ARE_SIGNIFICANT */
				{
				  while (word_end < word_start)
					{
					  *output++ = *word_end++;
					  column++;
					}
				}
			}

		  while (word_start < input)
			{
			  *output++ = *word_start++;
			  column++;
			}

		  word_end = input;
		  in_word = FALSE;

#ifdef NEWLINES_ARE_SIGNIFICANT
		  if (*input == CR || *input == LF)
			goto HARD_BREAK;

#else  /* !NEWLINES_ARE_SIGNIFICANT */

		  white_has_newline = (*input == CR || *input == LF);

		  /* Treat CRLF as CR */
		  if (white_has_newline && *input == CR && input[1] == LF)
			input++;

		  if (white_has_newline)
			{
			  input++;
# ifdef HACK_PREFIX
			  FIND_PREFIX ();
			  /* If the prefixes differ, treat this as a paragraph break. */
			  if (XP_STRCMP (new_prefix, last_prefix))
				goto PARA_BREAK;
			  XP_STRCPY (last_prefix, new_prefix);
# endif /* HACK_PREFIX */
			  continue;
			}
#endif /* !NEWLINES_ARE_SIGNIFICANT */

		  if (!*input)
			done = TRUE;
		}
	  else if (!in_word && !*input)
		{
		  break;
		}
	  else if (!in_word && (!XP_IS_SPACE (*input)))
		/* If we are between words and have reached non-whitespace,
		   remember this position in `input' as the beginning of the
		   word.
		 */
		{
		  in_word = TRUE;
		  word_start = input;
		}
	  else if (!in_word)
		{
		  if (*input == CR || *input == LF)
			{
#ifdef NEWLINES_ARE_SIGNIFICANT
	  HARD_BREAK:
# ifdef DEBUG_WRAP
		  printf ("HARD BREAK\n");
# endif /* DEBUG_WRAP */
		  in_word = FALSE;
		  column = 0;
		  while (word_end < input)	/* whitespace at eol */
			*output++ = *word_end++;
		  *output++ = *input++;
		  word_end = input;
		  word_start = input;

# ifdef HACK_PREFIX
		  s = input;
		  /* while (isspace (s)) s++; */
		  cited_line_p = (*s == '>');
# endif /* !HACK_PREFIX */

		  continue;

#else  /* !NEWLINES_ARE_SIGNIFICANT */

			  /* Treat CRLF as CR */
			  if (*input == CR && input[1] == LF)
				input++;

# ifdef HACK_PREFIX
			  input++;
			  FIND_PREFIX ();
			  input--;

			  /* If the prefixes differ, treat this as a paragraph break. */
			  if (XP_STRCMP (new_prefix, last_prefix))
				white_has_newline = TRUE;

		  PARA_BREAK:
			  XP_STRCPY (last_prefix, new_prefix);
# endif /* HACK_PREFIX */

			  if (white_has_newline)
				{
				  /* This is the second newline - paragraph break. */

				  column = 0;
				  for (s = LINEBREAK; *s; s++)
					*output++ = *s;
# ifdef HACK_PREFIX
				  for (s = new_prefix; *s; s++)
					*output++ = *s;
# endif /* !HACK_PREFIX */
				  for (s = LINEBREAK; *s; s++)
					*output++ = *s;
# ifdef HACK_PREFIX
				  for (s = new_prefix; *s; s++)
					*output++ = *s,
					column++;
# endif /* !HACK_PREFIX */
				  while (isspace (*input))
					input++;
				  white_has_newline = FALSE;
				  in_word = TRUE;
				  word_end = input;
				  word_start = input;
				  continue;
				}
			  else
				{
				  white_has_newline = TRUE;
				}
#endif /* !NEWLINES_ARE_SIGNIFICANT */
			}
		}

	  input++;
	}
  *output = 0;
#undef FIND_PREFIX

#ifdef DEBUG_WRAP
  memcpy (I, O, L);
#endif
}

#endif /* XP_WORD_WRAP_NEW_CODE */


#if 0

#include <stdio.h>

int
INTL_CharLen(int charsetid, unsigned char *str)
{
	return 1;
}

int
INTL_ColumnWidth(int charsetid, unsigned char *str)
{
	return 1;
}

void
main ()
{
  char *s;

#ifdef XP_WORD_WRAP_NEW_CODE
# define TEST(STR) \
  s = strdup (STR); \
  printf ("----------\n"); \
  printf ("%s\n", s); \
  printf ("----------\n"); \
  printf ("%s\n", XP_WordWrap(CS_LATIN1, s, 79, 1)); \
  printf ("----------\n"); \
  free (s)
#else
# define TEST(STR) \
  s = strdup (STR); \
  printf ("----------\n"); \
  printf ("%s\n", s); \
  XP_WordWrap (s, 79); \
  printf ("----------\n"); \
  printf ("%s\n", s); \
  printf ("----------\n"); \
  free (s)
#endif

#if 1
  TEST ("This is a test of the new wrapping code.  This test does not "
		"have any newlines in it, but it does have a number of long words "
		"like unfathomably and nevertheless and perspicacious.  Let's see "
		"how this works.  Let's try making it a bit longer.");

  TEST ("This is a test of the new wrapping code.  This test does not "
		"have any    newlines in it, but it does have a number of long words "
		"like unfathomably and nevertheless and perspicacious.  Let's see "
		"how this works.  Let's try making it a bit longer.       ");

  TEST ("This is a test of the new wrapping code.  This test does not "
		"have any    newlines in it, but it does have a number of long words "
		"like unfathomably and nevertheless and perspicacious.  Let's see "
		"how this works.  Let's try making it a bit longer.       "
		"                                      "
		);

  TEST ("This is a test of the new wrapping code.  This test does not "
		"have any    newlines in it, but it does have a number of long words "
		"like unfathomably and nevertheless and perspicacious.  Let's see "
		"how this works.  Let's try making it a bit longer.       "
		"                                                                  "
		);

  TEST ("This is a test of the new wrapping code.  This test DOES contain\n"
		"newlines, so let's see   \n"
		"how we deal with that.  And (for good measure) we'll throw in\n"
		"the afforementioned long words: unfathomably, nevertheless, and\n"
		" perspicacious.  Oh, and now `afforementioned.'");

  TEST ("This is a test of the new wrapping code.  This test DOES contain\n"
		"newlines, so let's see   \n"
		"how we deal with that.  And (for good measure) we'll throw in\n"
		"the afforementioned long words: unfathomably, nevertheless, and\n"
		" perspicacious.  Oh, and now `afforementioned.'\n"
		"\n"
		"And now we'll try multiple paragraphs.  This text should not\n"
		"be wrapped in with the preceeding text.  Blah blah blah.\n"
		"\n"
		"And now we'll try multiple paragraphs.  This text should not\n"
		"be wrapped in with the preceeding text.  Blah blah blah.\n"
		"   \n   \n   \n   \n   "
		"And now we'll try multiple paragraphs.  This text should not "
		"be wrapped in with the preceeding text.  Blah blah blah."
		);
#endif

  TEST (">> This is a test of the new wrapping code.  This test DOES contain\n"
		">> newlines, so let's see   \n"
		">> how we deal with that.  And (for good measure) we'll throw in\n"
		">> the afforementioned long words: unfathomably, nevertheless, and\n"
		">>   perspicacious.  Oh, and now `afforementioned.'\n"
		"\n"
		"And now we'll try multiple paragraphs.  This text should not\n"
		"be wrapped in with the preceeding text.  Blah blah blah.\n"
		"Blab blab blab.  Blab blab blab.  Blab blab blab.  Blab blab.\n"
		"\n"
		">  And now we'll try multiple paragraphs.  This text should not\n"
		">    be wrapped in with the preceeding text.  Blah blah blah.\n"
		">  Blab blab blab.  Blab blab blab.  Blab blab blab.  Blab blab.\n"
		"   \n   \n   \n   \n   "
		"And now we'll try multiple paragraphs.  This text should not "
		"be wrapped in with the preceeding text.  Blah blah blah."
		"Blab blab blab.  Blab blab blab.  Blab blab blab.  Blab blab.\n"
		"\n"
		"> And here's a line that begins with > in column 0 which is longer"
		" than the fill column.  This line should be left alone.\n"
		"> This is the second such line: it's short.\n"
		">> This is the third such line, but much like the first line, it"
		" is long.  Run and hide.  Run and hide.  Run and hide.\n"
		"> You can hide.  All your life.  Run and hide.  Run and hide.  "
		"but I'll have.  Your hide.  Run and hide.  Run and hide.\n"
		"\n"
		"This line doesn't begin with > and is long and thus should be"
		" wrapped (like a good line should.)  But this next line:\n"
		">does begin with that hateful > character, and should be left"
		" alone no matter how it rambles on.  No escape.  run and hide.\n"
		"And now we're back in wrappable line land.  On.  And on and on.  "
		"And on and on.  And on.\n"
		"\n"
		"And now a test of a word that is longer than the line: "
		"12345689-12345689-12345689-12345689-12345689-12345689-12345689-"
		"12345689-12345689-12345689-12345689-12345689-12345689-12345689.  "
		"12345689-12345689-12345689-12345689-12345689-12345689-12345689-"
		"12345689-12345689-12345689-12345689-12345689-12345689-12345689."
		);

# undef TEST
}

#endif /* 0 */
