/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "xlate_i.h"
#include "ctxtfunc.h"
#include "net.h"
#include "libi18n.h"
#include "fe_proto.h"
#include "prefapi.h"
#include "intl_csi.h"

#define LLOYD_HACK	1

/*
** This code is sometimes called "The Text Front End", because it is a set
** of front end routines that deal with conversion of HTML to text, but it
** is more accurately the "Plain Text Converter".  IMHO a "Text Front End"
** is something like Lynx.
**
** The basic strategy for the text front end is to collect bits in an internal
** buffer until it is known that no more data will be written into the area
** covered by that buffer, and at that point, output all the characters in
** the buffer.
**
** So far this file is almost entirely Michael Toy's fault.  Eventually
** someone else will have to own this code, since Michael is showing
** signs of having been lobotomized.
*/

/*
** 11-20-96 pchen
**
** Added some XP_MAC's because we need construction of our new front end
** context object.
**
** ****************** NOTE! ******************
** If we ever override more context functions (i.e. implement stubbed
** out TXFE_* functions in stubs.c), you need to let a MacFE person
** know about the changes.
**
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XP_MAC
/* MacFE callbacks */
extern MWContext* CreatePlainTextConversionContext(MWContext* inUIContext);
extern void DisposePlainTextConversionContext(MWContext* inContext);
#endif /* XP_MAC */

/*
** This is the structure used to cache previously written on lines until
** such a time as it looks like they are needed.
*/
struct LineRecord_struct {
  struct LineRecord_struct *next;
  char *buffer;
  int  buffer_size;
  int y_top;
};

/*
** prepare_to_render
**	This routine makes sure that that rendering at "y" can be done
**	by simply ignoring y and writing wherever the x value says to.
**
**	What it does is build a singly linked list of LineRec's,
**	and sets the cs->prInfo->line to point to the buffer field
**	of the appropriate LineRecord.
*/
PRIVATE void
prepare_to_render(MWContext* cx, int y)
{
  LineRecord *cur, *lr, *last;

  /*
  ** Search for a line which covers this area already
  */
  last = cur = NULL;
  for (lr = cx->prInfo->saved_lines; lr != NULL; lr = lr->next) {
    if (y+TEXT_HEIGHT/2 >= lr->y_top && y+TEXT_HEIGHT/2 < lr->y_top+TEXT_HEIGHT)
    {
      cur = lr;
      break;
    }
    if (y+TEXT_HEIGHT/2 < lr->y_top)
      break;
    last = lr;
  }
  
  if (cur == NULL) {
    /*
    ** Need to add a new line structure which covers the area spanned
    ** by this item.
    */
    if ((cur = XP_NEW(LineRecord)) == NULL) {
      cx->prInfo->line = NULL;
      return;
    }

    cur->y_top = y;
    if ((cur->buffer = (char *) XP_ALLOC(LINE_WIDTH)) == NULL) {
      XP_DELETE(cur);
      cx->prInfo->line = NULL;
      return;
    }

    cur->next = NULL;
    XP_MEMSET(cur->buffer, ' ', LINE_WIDTH);
    cur->buffer_size = LINE_WIDTH;

    if (last == NULL) {
      cur->next = cx->prInfo->saved_lines;
      cx->prInfo->saved_lines = cur;
    } else {
      cur->next = last->next;
      last->next = cur;
    }
  }
  
  /*
  ** In either case, "cur" now points to the LineRecord for the
  ** current item.
  */
  cx->prInfo->line = cur->buffer;
}

PRIVATE void
default_completion(PrintSetup* p)
{
  XP_FileClose(p->out);
}

PRIVATE void
tx_alldone(URL_Struct *URL_s, int status, MWContext *window_id)
{
  window_id->prSetup->status = status;
}

PUBLIC void
XL_InitializeTextSetup(PrintSetup *p)
{
  XP_BZERO(p, sizeof *p);
  p->width = 76;  /* don't use 79 here, or adding ">" makes it wrap. -jwz */
  p->prefix = "";
  p->eol = LINEBREAK;
  p->completion = default_completion;
}

PRIVATE void
real_discard_translation_context(void *vcx)
{
  MWContext *cx = (MWContext *)vcx;

  if (cx) {
    if (cx->prInfo) {
      XP_DELETE(cx->prInfo);
    }
    if (cx->prSetup) XP_DELETE(cx->prSetup);
#ifndef XP_MAC
    if (cx->funcs) XP_DELETE(cx->funcs);
    XP_DELETE(cx);
#else
    DisposePlainTextConversionContext(cx);
#endif /* ndef XP_MAC */
  }
}

/*
 *  Hack alert.
 *  06-21-96 GAB 3.0 Beta 5 Critical Bug #23283
 *
 *  When translating text in the composition window (quoting a
 *      message) and the window is closed, this context is
 *      interrupted.  On interruption, AllConnectionsComplete
 *      gets called, which in turn calls this function.
 *  Previous incantation deleted the context and it's data; upon
 *      return to NET_InterruptWindow, the netlib attempts to
 *      reference the context (now freed), and we have an
 *      access violation.
 *  In order to avoid to problem, we set a timeout to delete
 *      ourselves at a later time (2 seconds later).
 */
PRIVATE void
discard_translation_context(MWContext *cx)
{
  FE_SetTimeout(real_discard_translation_context, (void *)cx, 2000);
}

PUBLIC void TXFE_AllConnectionsComplete(MWContext *cx) 
{
  (*cx->prSetup->completion)(cx->prSetup);
  LO_DiscardDocument(cx);
  discard_translation_context(cx);
}

MODULE_PRIVATE void
TXFE_SetCallNetlibAllTheTime(MWContext *cx)
{
  (*cx->prInfo->scnatt)(NULL);
}

MODULE_PRIVATE void
TXFE_ClearCallNetlibAllTheTime(MWContext *cx)
{
  (*cx->prInfo->ccnatt)(NULL);
}

PUBLIC void
XL_AbortTextTranslation(XL_TextTranslation tt)
{
  MWContext* cx = (MWContext*) tt;
  NET_InterruptStream(cx->prSetup->url);
}

PUBLIC XL_TextTranslation
XL_TranslateText(MWContext* cx, URL_Struct *url_struct, PrintSetup* pi)
{
  int nfd;
#ifndef XP_MAC
  MWContext *text_context = XP_NewContext();
#else
  MWContext *text_context = CreatePlainTextConversionContext(cx);
#endif /* ndef XP_MAC */
  XP_Bool citation_p;
  INTL_CharSetInfo text_csi;

  if (text_context == NULL || pi == NULL || pi->out == NULL)
    return NULL;

  text_csi = LO_GetDocumentCharacterSetInfo(text_context);
  text_context->type = MWContextText;
  text_context->prInfo = XP_NEW(PrintInfo);
#ifndef XP_MAC
  text_context->funcs = XP_NEW(ContextFuncs);
#endif /* ndef XP_MAC */
  text_context->prSetup = XP_NEW(PrintSetup);
  if (text_context->funcs == NULL || text_context->prSetup == NULL
      || text_context->prInfo == NULL)
  {
    discard_translation_context(text_context);
    return NULL;
  }

#ifndef XP_MAC
#define MAKE_FE_FUNCS_PREFIX(f) TXFE_##f
#define MAKE_FE_FUNCS_ASSIGN text_context->funcs->
#include "mk_cx_fn.h"
#endif /* ndef XP_MAC */

  text_context->convertPixX = 1;
  text_context->convertPixY = 1;
  text_context->prInfo->page_width = pi->width * TEXT_WIDTH;
  if (cx && cx->funcs) {
    text_context->prInfo->scnatt = cx->funcs->SetCallNetlibAllTheTime;
    text_context->prInfo->ccnatt = cx->funcs->ClearCallNetlibAllTheTime;
  }
  *text_context->prSetup = *pi;
  text_context->prInfo->page_break = 0;
#ifdef LLOYD_HACK
  text_context->prInfo->in_table = TRUE;
#else
  text_context->prInfo->in_table = FALSE;
#endif
  text_context->prInfo->first_line_p = TRUE;
  text_context->prInfo->saved_lines = NULL;
  text_context->prInfo->line = NULL;
  text_context->prInfo->last_y = 0;

  text_context->prSetup->url = url_struct;

#ifndef XP_MAC
  INTL_SetCSIDocCSID(text_csi, INTL_DefaultDocCharSetID(cx));	/* For char code conversion	*/
  INTL_SetCSIWinCSID(text_csi, INTL_DefaultWinCharSetID(cx));	/* For char code conversion	*/
#endif /* ndef XP_MAC */

  url_struct->position_tag = 0;

  /* Assume this is a news or mail citation if there is a prefix string.
     We use a different format-out in this case so that mknews.c can be
     more clever about this sort of thing. */
  citation_p = pi->prefix && *pi->prefix;

  /* strip off any named anchor information so that we don't
   * truncate the document when quoting or saving
   */
  if(url_struct->address)
  	XP_STRTOK(url_struct->address, "#");
  nfd = NET_GetURL (url_struct,
		    (citation_p ? FO_QUOTE_MESSAGE : FO_SAVE_AS_TEXT),
		    text_context,
		    tx_alldone);

  /* 5-31-96 jefft -- bug # 21519
   * If for some reason we failed on getting URL return NULL.
   * At this point text_context has been free'd somewhere down the
   * chain through discard_translation_context(cx). text_context
   * became invalid.
   */
  if ( nfd < 0 ) return NULL;

  return text_context;
}

PRIVATE void
measure_text(LO_TextStruct *text, LO_TextInfo *text_info, int start, int end)
{
    text_info->ascent = TEXT_HEIGHT/2;
    text_info->descent = TEXT_HEIGHT - text_info->ascent;
    text_info->max_width = TEXT_WIDTH*(end-start+1);
    text_info->lbearing = 0;
    text_info->rbearing = 0;
}

MODULE_PRIVATE int
TXFE_GetTextInfo(MWContext *cx, LO_TextStruct *text, LO_TextInfo *text_info)
{
    measure_text(text, text_info, 0, text->text_len-1);
    return 0;
}

MODULE_PRIVATE void
TXFE_LayoutNewDocument(MWContext *cx, URL_Struct *url, int32 *w, int32 *h, int32* mw, int32* mh)
{
    *w = cx->prInfo->page_width;
    *h = 0;
    *mw = *mh = 0;
}

PRIVATE void
text_at(MWContext *cx, int x, char *str, int len)
{
  int line_width =  LINE_WIDTH;

    if (cx->prInfo->line == NULL) {
      /*
      ** Something is wrong, either prepare_to_render wasn't called,
      ** which is a bug, or prepare_to_render failed to malloc enough
      ** space for the line buffer.
      */
      return;
    }

	/* x counts for number of bytes previously wrote to the line */
    x = (x + TEXT_WIDTH/2) / TEXT_WIDTH;

    /***** 5/26/96 jefft -- bug# 21360
     * if the len is >= to the default LINE_WIDTH we have to
     * reallocate the line buffer. Otherwise, we will end up truncating
     * the message. That would be really bad.
     */
    if ( len + x >= LINE_WIDTH ) {
      register LineRecord *cur = NULL;

	  line_width = len + x;

      for ( cur = cx->prInfo->saved_lines; cur; cur = cur->next ) {
		if ( cur->buffer == cx->prInfo->line ) break;
      }

      XP_ASSERT (cur);
      /* don't really know this is right */
      if ( !cur) return;
      /* plus one accounts for the null character at the end */
      cx->prInfo->line = XP_REALLOC (cx->prInfo->line, line_width+1);
      if ( cx->prInfo->line == NULL ) return;
	  /* starts with +x; we don't want to wipe out the previously wrote
	   * strings, i.e. "< " quote strings
	   */
      XP_MEMSET ( cx->prInfo->line+x, ' ', len+1 );
      cur->buffer = cx->prInfo->line;
      cur->buffer_size = line_width+1;
    }
	
    while (x < line_width && len > 0) {
	cx->prInfo->line[x] = *str++;
	x++;
	len--;
    }
}

MODULE_PRIVATE void
TXFE_DisplaySubtext(MWContext *cx, int iLocation, LO_TextStruct *text,
		    int32 start_pos, int32 end_pos, XP_Bool notused)
{
    int x;
    LO_TextInfo i;
    x = text->x + text->x_offset;
    measure_text(text, &i, 0, start_pos-1);	/* XXX too much work */
    if (start_pos) {
	x += i.max_width;
    }
    prepare_to_render(cx, text->y);
    text_at(cx, x, (char*) text->text, end_pos - start_pos + 1);
}

MODULE_PRIVATE void
TXFE_DisplayText(MWContext *cx, int iLocation, LO_TextStruct *text, XP_Bool needbg)
{
    (cx->funcs->DisplaySubtext)(cx, iLocation, text, 0, text->text_len-1, needbg);
}

MODULE_PRIVATE void
TXFE_DisplaySubDoc(MWContext *cx, int iLocation, LO_SubDocStruct *subdoc_struct)
{
    return;
}

/*
** writeln
**	Evidence that I once programmed in Pascal :).  This routine
**	writes out the passed buffer, prepending the correct prefix,
**	stripping trailing spaces, and appending the correct line
**	termination;
*/
PRIVATE void
writeln(MWContext *cx, char* s, int line_width)
{
  char *buf = NULL;
  char *cp;
  char *prefix = cx->prSetup->prefix;
  INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cx);
  int16 win_csid = INTL_GetCSIWinCSID(csi);

#ifdef USE_LINE_WIDTH
  assert(cx->prSetup->prefix==NULL || strlen(cx->prSetup->prefix)<=LINE_WIDTH);
#else
  assert(cx->prSetup->prefix==NULL || strlen(cx->prSetup->prefix)<=line_width); 
#endif

  buf = (char *) XP_ALLOC ( line_width << 1 );

  /* If this is the first line, and the document is of type message/rfc822,
     and there is a prefix, then we are in the ``Quote Document'' code for
     a news or mail message.  This means that the message/rfc822 converter
     is going to emit a line like ``In <blah> foo@bar.com wrote:'' and we
     should not prepend that line with "> ".
   */
  if (cx->prInfo->first_line_p)
    {
      if (cx->prSetup->url &&
	  (!strcasecomp (cx->prSetup->url->content_type, MESSAGE_RFC822) ||
	   !strcasecomp (cx->prSetup->url->content_type, MESSAGE_NEWS)))
	prefix = 0;
      cx->prInfo->first_line_p = FALSE;
    }

  if (s != NULL) {
    for (cp = s+line_width-2; cp >= s; cp--)
      if (*cp != ' ') {
	cp[1] = '\0';
	break;
      }
  }
  if (prefix)
    XP_STRCPY(buf, prefix);
  else
    *buf = 0;
  if (s != NULL && cp >= s)
    XP_STRCAT(buf, s);
  XP_STRCAT(buf, cx->prSetup->eol);

  /* Convert nobreakspace to space.
     Maybe this is wrong... */
/*
	<ftang>
	Answer: Yes, this is wrong.
		\240 (0xA0) is not nobreakspace in many encoding.
		It could be 
		DAGGER in MacRoman
		LATIN CAPITAL LETTER YACUTE in MacRoman Icelandic and Faroese variations
		characters in any other Macintosh character set.
		The second byte of ShiftJIS characters

		See Bug # 6640
		
  for (s = buf; *s; s++)
    if (*((unsigned char *) s) == '\240')
      *s = ' ';
*/
/* 
void INTL_StripNBSP(int16 csid, char* buf)
*/
 if (CS_USER_DEFINED_ENCODING != win_csid)
 { /* strip out NBSP */
  	const char* nbsp = INTL_NonBreakingSpace(win_csid);
  	int nbsplen = XP_STRLEN(nbsp);
  	char* out;
  	char* next;
  	char* boundary = buf + XP_STRLEN(buf) - nbsplen;
  	XP_ASSERT((win_csid != CS_DEFAULT) && (win_csid != CS_UNKNOWN));
	for (s = out = buf; *s;)
	{
		char* next = INTL_NextChar(win_csid, s);
		if( (s <= boundary)  && (0 == XP_STRNCMP(s, nbsp, nbsplen)))
		{ /* if it is nbsp, replace it with space and advance the pointer */
			*out = ' ';
			out++;
			s = next;
		}
		else
		{ /* if it is not nbsp, copy it */
			for( ;s < next; s++, out++)
				*out = *s;
		}
	}
	*out = '\0'; /* NULL terminate it*/
  }
  XP_FileWrite(buf, XP_STRLEN(buf), cx->prSetup->out);

  XP_FREE (buf);
}

/*
** expire_lines
**	Write out data from the cache.  If expire_all is TRUE
**	then that is the secret code to dump the entire cache (at the end
**	of the document.
*/
PRIVATE void
expire_lines(MWContext *cx, int bottom, XP_Bool expire_all)
{
  LineRecord *cur, *next;

  for (cur = (LineRecord *)cx->prInfo->saved_lines; cur != NULL; cur = next) {
    if (!expire_all && cur->y_top > bottom)
      return;
    cx->prInfo->saved_lines = (LineRecord *)cur->next;
    next = cur->next;
    if ((cur->y_top - cx->prInfo->last_y) > TEXT_HEIGHT/2)
      writeln(cx, NULL, LINE_WIDTH);
    writeln(cx, cur->buffer, cur->buffer_size);
    cx->prInfo->last_y = cur->y_top + TEXT_HEIGHT;
    XP_FREE(cur->buffer);
    XP_FREE(cur);
  }
}

/*
** TXFE_DisplayTable
**	This routine arranges that lines will be cached for the entire
**	span of the table, since table data is not drawn from top
**	top bottom.
**
**	It is SUPPOSED to draw borders, but doesn't, mostly because
**	there is no way to tell the layout engine that borders are
**	the thickness of a character.
*/

MODULE_PRIVATE
void TXFE_DisplayTable(MWContext *cx, int iLoc, LO_TableStruct *table)
{
  if (cx->prInfo->in_table)
    return;
  cx->prInfo->table_top = table->y;
  cx->prInfo->table_bottom = table->y + table->line_height;
#ifndef LLOYD_HACK
  cx->prInfo->in_table = TRUE;
#endif
}

/*
** TXFE_DisplayLineFeed
**	Here it is that text actually gets emitted from the translation.
** 	If there is reason to believe that some of the old cached lines are
**	'finished', those lines are written out and wiped from the cache.
*/

MODULE_PRIVATE void
TXFE_DisplayLineFeed(MWContext *cx, int iLocation, LO_LinefeedStruct *line_feed, XP_Bool notused)
{
  int my_bottom = line_feed->y + line_feed->line_height;
  if (cx->prInfo->in_table) {
#ifndef LLOYD_HACK
    if (line_feed->y >= cx->prInfo->table_bottom)
    {
      expire_lines(cx, cx->prInfo->table_bottom, FALSE);
      cx->prInfo->in_table = FALSE;
    }
#endif
  } else {
    expire_lines(cx, my_bottom, FALSE);
  }
}

/*
** TXFE_DisplayHR
**	XXX I want this routine to use "=" if the HR is a "thick" one.
**
*/
MODULE_PRIVATE void
TXFE_DisplayHR(MWContext *cx, int iLocation , LO_HorizRuleStruct *HR)
{
  int x, left;
  int32 width = 72;

  /*
  ** jefft -- You may say this is a hack. Indeed, it is a hack. In order
  ** to preserve the plain text line format when quoting we set the print->width
  ** with a extreme large number 998 so that we won't have long/short wrapping
  ** pattern (refer to lib/libmsg/msgcpane.cpp MSG_CompositionPane::QuoteMessage())
  ** This causes problem when quoting a multipart/mixed text/plain message + 
  ** text/plain attachment. It creates an extremely long line of separator. 
  ** It seems reasonable to do some adjustment here to limit the width of 
  ** the separator to the setting of current wrap long line length.
  **/
  PREF_GetIntPref("mailnews.wraplength", &width);

  width *= TEXT_WIDTH; /* in pixel */
  left = x = (TEXT_WIDTH << 1) + HR->x_offset;

  if (HR->width < width) {
	  /* BUG 83737 -- since the line separator has been centered within
	  ** the window's coordinate we might as well adjust the start of the
	  ** x.
	  */
	  width = HR->width;
	  left = x = HR->x + HR->x_offset;
  }

  prepare_to_render(cx, HR->y);
  for (; x < left + width; x += TEXT_WIDTH)
    text_at(cx, x, "-", 1);
}

/*
** TXFE_TranslateISOText
**	This assumes the text is already ISO.  This is wrong, it should call
**	the correct front end translation routine.  I have some vague memory
**	that perhaps the Mac front end patches out this function pointer
**	but it is something which should be looked into
*/

MODULE_PRIVATE char *
TXFE_TranslateISOText(MWContext *cx, int charset, char *ISO_Text)
{
    return ISO_Text;
}

/*
** XL_DisplayTextImage
**	Just display the alt text.  It currently displays in the upper left
**	of the image rectangle.  Perhaps in future it should try and center
**	the alt text in the image.
*/
PUBLIC void
XL_DisplayTextImage(MWContext *cx, int iLocation, LO_ImageStruct *img)
{
    int x = img->x + img->x_offset;
    char *p;

    prepare_to_render(cx, img->y);
    if (img->alt_len > 0) {
      PA_LOCK(p, char*, img->alt);
      if (*p != '[')
	text_at(cx, x, "[", 1);
      text_at(cx, x+TEXT_WIDTH, p, img->alt_len);
      if (p[img->alt_len-1] != ']')
	text_at(cx, x+TEXT_WIDTH*(img->alt_len+1), "]", 1);
      PA_UNLOCK(img->alt);
    }
}

/*
** XL_GetTextImage
**	All images are ignored in the text front end.  This routine is
**	just here to insure that layout doesn't block and that all images
**	at least have the default ALT tag text.
*/
PRIVATE char img[] = "Image";
PUBLIC void
XL_GetTextImage(LO_ImageStruct *image)
{
    if (image->alt_len == 0)
      if (image->alt == NULL)
      {
	/*
	** No ALT tag specified, insert  make it appear as [Image]
	*/
	char *p;
	if ((image->alt = PA_ALLOC((sizeof img) - 1)) == NULL) {
	    image->width = 1;
	    image->height = 1;
	    return;
	}
	image->alt_len = (sizeof img) - 1;
	p = PA_LOCK(p, char*, image->alt);
	XP_MEMMOVE(p, img, (sizeof img) - 1);
	PA_UNLOCK(image->alt);
      }
      else
      {
	image->width = 1;
	image->height = 1;
      }
    image->width = TEXT_WIDTH*(2+image->alt_len);
    image->height = TEXT_HEIGHT;
}

/*
** TXFE_DisplayBullet
**
**	The only magic here is the calculation about how far to back up
**	in order to get the bullet at the correct location.  The calculation
**	is based on reading the layout code for bulleted lists.
*/
MODULE_PRIVATE void
TXFE_DisplayBullet(MWContext *cx, int iLocation, LO_BullettStruct *bullet)
{
  int x = bullet->x + bullet->x_offset;

  prepare_to_render(cx, bullet->y);
  x += 2*bullet->width;
  x -= 2*TEXT_WIDTH;
  switch (bullet->bullet_type) {
    case BULLET_ROUND:
      text_at(cx, x, "o", 1);
      break;
    case BULLET_BASIC:
      text_at(cx, x, "*", 1);
      break;
    case BULLET_SQUARE:
      text_at(cx, x, "+", 1);
      break;
    case BULLET_MQUOTE:
      text_at(cx, x, ">", 1);
      break;
  }
}

MODULE_PRIVATE void
TXFE_DisplayBorder(MWContext *context, int iLocation, int x, int y, int width,
                   int height, int bw, LO_Color *color, LO_LineStyle style)
{
}


MODULE_PRIVATE void
TXFE_DisplayFeedback(MWContext *context, int iLocation, LO_Element *element)
{
}

MODULE_PRIVATE void
TXFE_FinishedLayout(MWContext *cx)
{
  expire_lines(cx, 0, TRUE);
}

MODULE_PRIVATE char *
TXFE_PromptPassword(MWContext *cx, const char *msg)
{
  MWContext *wincx = cx->prSetup->cx;
  /* If we don't have a context, go find one. This is really a hack, we may
   * attach to some random window, but at least the print/saveas/etc will
   * still work. Our caller can prevent this by actually setting cx->prSetup->cx
   * to something that will really stay around until the save or print is
   * complete */
  if (wincx == NULL) {
	wincx = XP_FindSomeContext();
  }

  if (wincx != NULL)
    return FE_PromptPassword(wincx, msg);

  return NULL;
}

#ifdef __cplusplus
}
#endif
