/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/*
** This file is mis-named.  It is the main body of code for
** dealing with PostScript translation.  It was originally
** written by Michael Toy, but now that you have read this
** comment, all bugs in this code will be automatically
** assigned to you.
*/

#include "xlate_i.h"
#include "xplocale.h"
#include "libimg.h"             /* Image Library public API. */

#ifdef X_PLUGINS
#include "np.h"
#endif /* X_PLUGINS */
#include <libi18n.h>
#include "il_util.h"
#include "intl_csi.h"

#ifdef JAVA
#include "java.h"
#endif

#define THICK_LINE_THICKNESS    10
#define THIN_LINE_THICKNESS     5

#define LIGHTER_GREY            10
#define LIGHT_GREY              8
#define GREY                    5
#define DARK_GREY               2
#define DARKER_GREY             0

PRIVATE void
default_completion(PrintSetup *p)
{
  XP_FileClose(p->out);
  p->out = NULL;
}

typedef struct {
    int32	start;
    int32	end;
    float	scale;
} InterestingPRE;

static int small_sizes[7] = { 6, 8, 10, 12, 14, 18, 24 };
static int medium_sizes[7] = { 8, 10, 12, 14, 18, 24, 36 };
static int large_sizes[7] = { 10, 12, 14, 18, 24, 36, 48 };
static int huge_sizes[7] = { 12, 14, 18, 24, 36, 48, 72 };

MODULE_PRIVATE int
scale_factor(MWContext *cx, int size, int fontmask)
{
  int bigness;

  size--;
  if (size >= 7)
    size = 6;
  if (size < 0)
    size = 0;
  if (cx->prSetup->sizes)
    return cx->prSetup->sizes[size];
  bigness = cx->prSetup->bigger;
  if (fontmask & LO_FONT_FIXED)
    bigness--;
  switch (bigness)
    {
    case -2: return small_sizes[size];
    case -1: return small_sizes[size];
    case 0: return medium_sizes[size];
    case 1: return large_sizes[size];
    case 2: return huge_sizes[size];
    }
  return 12;
}

MODULE_PRIVATE void
PSFE_SetCallNetlibAllTheTime(MWContext *cx)
{
  if (cx->prInfo->scnatt)
    (*cx->prInfo->scnatt)(NULL);
}

MODULE_PRIVATE void
PSFE_ClearCallNetlibAllTheTime(MWContext *cx)
{
  if (cx->prInfo->ccnatt)
    (*cx->prInfo->ccnatt)(NULL);
}

PUBLIC void
XL_InitializePrintSetup(PrintSetup *p)
{
    XP_BZERO(p, sizeof *p);
    p->right = p->left = .75 * 72;
    p->bottom = p->top = 72;

    p->width = 72 * 8.5;
    p->height = 72 * 11;

    p->reverse = FALSE;
    p->color = TRUE;
    p->landscape = FALSE;
    p->n_up = 1;
    p->bigger = 0;
    p->completion = default_completion;
    p->dpi = 96.0;
    p->underline = TRUE;
    p->scale_pre = TRUE;
    p->scale_images = TRUE;
    p->rules = 1.0;
    p->header = "%t%r%u";
    p->footer = "%p%r%d";
}

/*
** Default completion routine for process started with NET_GetURL.
** Just save the status.  We will be called again when all connections
** associated wiht this context chut down, and there is where the
** status variable is used.
*/
PRIVATE void
ps_alldone(URL_Struct *URL_s, int status, MWContext *window_id)
{
    window_id->prSetup->status = status;
}

/*
** Called when everything is done and the whole doc has been fetched
** successfully. It draws the document one page at a time, in the order
** specified by the user.
**
** XXX Problems
**	If items fetched properly the first time, but not the second,
**	that fact is never ever passed back to the user.  This might
**	occur if, for example the doc and all it's images were bigger than
**	the caches, and the host machine went down between the first
**	and second passes.
*/
PRIVATE void xl_generate_postscript(MWContext *cx)
{
    int pn;

    /*
    ** This actually makes a second pass through the document, finding
    ** the appropriate page breaks.
    */
    XP_LayoutForPrint(cx, cx->prInfo->doc_height);

    /*
    ** Pass 3.  For each page, generate the matching postscript.
    */
    cx->prInfo->phase = XL_DRAW_PHASE;
    xl_begin_document(cx);
    for (pn = 0; pn < cx->prInfo->n_pages; pn++) {
	int page;
 	if (cx->prSetup->reverse)
	    page = cx->prInfo->n_pages - pn - 1;
	else
	    page = pn;
	xl_begin_page(cx, page);
	XP_DrawForPrint(cx, page);
	xl_end_page(cx,page);
    }
    xl_end_document(cx);
}

/*
** XL_TranslatePostscript:
** 	This is the main routine for postscript translation.
**
**	context - Front end context containing a function pointer table funcs,
**		   used to learn how to bonk the netlib when appropriate.
**	url - This is the URL that is to be printed.
**	pi - This is a structure describing all the options for controlling
**	     the translation process.
**
** XXX This routine should check for errors (like malloc failing) and have
** a return value which indicates whether or not a translation was actually
** going to happen.
**
** The completion routine in "pi" is called when the translation is complete.
** The value passed to the completion routine is the status code passed by
** netlib when the docuement and it's associated files are all fetched.
*/

PUBLIC void
XL_TranslatePostscript(MWContext *context, URL_Struct *url_struct, SHIST_SavedData *saved_data, PrintSetup *pi)
{
    int nfd;
    MWContext* print_context = XP_NewContext();
    uint32 display_prefs;
    IL_DisplayData dpy_data;
    ContextFuncs *cx_funcs = context->funcs;
    int16 print_doc_csid;
    INTL_CharSetInfo print_csi = LO_GetDocumentCharacterSetInfo(print_context);

    print_context->type = MWContextPostScript;

    /* inherit character set */
    print_doc_csid = INTL_DefaultDocCharSetID(context);
    INTL_SetCSIDocCSID(print_csi, print_doc_csid);
    INTL_SetCSIWinCSID(print_csi, INTL_DocToWinCharSetID(print_doc_csid));

    print_context->funcs = XP_NEW(ContextFuncs);
#define MAKE_FE_FUNCS_PREFIX(f) PSFE_##f
#define MAKE_FE_FUNCS_ASSIGN print_context->funcs->
#include "mk_cx_fn.h"

    /* Copy the form data into the URL struct. */
    if (saved_data->FormList)
        LO_CloneFormData(saved_data, print_context, url_struct);
  
    /*
    ** Coordinate space is 720 units to the inch.  Space we are converting
    ** from is "screen" coordinates, which we will assume for the sake of
    ** argument to be 72 dpi.  This is just a misguided attempt to account
    ** for fractional character placement to give more pleasing text layout.
    */
    print_context->convertPixX = INCH_TO_PAGE(1) / pi->dpi;
    print_context->convertPixY = INCH_TO_PAGE(1) / pi->dpi;
    xl_initialize_translation(print_context, pi);
    pi = print_context->prSetup;
    XP_InitializePrintInfo(print_context);
    /* Bail out if we cannot get memory for print info */
    if (print_context->prInfo == NULL)
	return;
    print_context->prInfo->phase = XL_LOADING_PHASE;
    print_context->prInfo->page_width = (pi->width - pi->left - pi->right);
    print_context->prInfo->page_height = (pi->height - pi->top - pi->bottom);
    print_context->prInfo->page_break = print_context->prInfo->page_height;
    print_context->prInfo->doc_title = XP_STRDUP(url_struct->address);
#ifdef LATER
    print_context->prInfo->interesting = NULL;
    print_context->prInfo->in_pre = FALSE;
#endif
    if (cx_funcs) {
	print_context->prInfo->scnatt = cx_funcs->SetCallNetlibAllTheTime;
	print_context->prInfo->ccnatt = cx_funcs->ClearCallNetlibAllTheTime;
    }
    
    if (pi->color && pi->deep_color) {
        IL_RGBBits rgb;

        rgb.red_bits = 8;
        rgb.red_shift = 16;
        rgb.green_bits = 8;
        rgb.green_shift = 8;
        rgb.blue_bits = 8;
        rgb.blue_shift = 0;
        if (!print_context->color_space)
            print_context->color_space = IL_CreateTrueColorSpace(&rgb, 32);
    }
    else {
        IL_RGBBits rgb;

        rgb.red_bits = 4;
        rgb.red_shift = 8;
        rgb.green_bits = 4;
        rgb.green_shift = 4;
        rgb.blue_bits = 4;
        rgb.blue_shift = 0;
        if (!print_context->color_space)
            print_context->color_space = IL_CreateTrueColorSpace(&rgb, 16);
    }

    /* Create and initialize the Image Library JMC callback interface.
       Also create an IL_GroupContext for the current context. */
    if (!psfe_init_image_callbacks(print_context)) {
        /* This can only happen if we are out of memory.  Abort printing
           and return. */
        XP_FREE(print_context->prInfo->doc_title);
        XP_CleanupPrintInfo(print_context);
        xl_finalize_translation(print_context);
        IL_ReleaseColorSpace(print_context->color_space);
        XP_DELETE(print_context->funcs);
        XP_DELETE(print_context);
        return;
    }

    dpy_data.color_space = print_context->color_space;
    dpy_data.progressive_display = FALSE;
    display_prefs = IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY;
    IL_SetDisplayMode (print_context->img_cx, display_prefs, &dpy_data);

#ifndef M12N                    /* XXXM12N Obsolete */
    IL_DisableLowSrc (print_context);
#endif /* M12N */
    print_context->prSetup->url = url_struct;
    print_context->prSetup->status = -1;
    url_struct->position_tag = 0;

	/* strip off any named anchor information so that we don't
	 * truncate the document when quoting or saving
	 */
	if(url_struct->address)
		XP_STRTOK(url_struct->address, "#");

    nfd = NET_GetURL (url_struct,
	      FO_SAVE_AS_POSTSCRIPT,
	      print_context,
	      ps_alldone);
}

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static void
measure_non_latin1_text(MWContext *cx, PS_FontInfo *f, unsigned char *cp,
	int start, int last, float sf, LO_TextInfo *text_info)
{
	int	charSize;
	int	height;
	int	i;
	int	left;
	int	right;
	int	square;
	int	width;
	int	x;
	int	y;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cx);
	int16 win_csid = INTL_GetCSIWinCSID(csi);

	width = height = left = right = x = y = 0;

	square = cx->prSetup->otherFontWidth;
	text_info->ascent = max(f->fontBBox.ury, cx->prSetup->otherFontAscent) * sf;

	i = start;
	while (i <= last)
	{
		if ((*cp) & 0x80)
		{
			while ((i <= last) && ((*cp) & 0x80))
			{
				if (INTL_IsHalfWidth(win_csid, cp))
				{
					width = x + square/2;
					right = max(right, x + square*45/100);
					x += square/2;
				}
				else
				{
					width = x + square;
					right = max(right, x + square*9/10);
					x += square;
				}
				height = max(height, y + square*7/10);
				left = min(left, x + square/25);
				charSize = INTL_CharLen(win_csid, cp);
				i += charSize;
				cp += charSize;
			}
		}
		else
		{
			while ((i <= last) && (!((*cp) & 0x80)))
			{
				PS_CharInfo *temp;
	
				temp = f->chars + *cp;
				width = max(width, x + temp->wx);
				height = max(height, y + temp->charBBox.ury);
				left = min(left, x + temp->charBBox.llx);
				right = max(right, x + temp->charBBox.urx);
				x += temp->wx;
				y += temp->wy;
				i++;
				cp++;
			}
		}
	}
	/*
	text_info->max_height = height * sf;
	*/
	text_info->max_width = width * sf;
	text_info->lbearing = left * sf;
	text_info->rbearing = right * sf;
}


PRIVATE void
ps_measure_text(MWContext *cx, LO_TextStruct *text,
		LO_TextInfo *text_info, int start, int last)
{
    PS_FontInfo *f;
    float sf;
    int x, y, left, right, height, width;
    int i;
    unsigned char *cp;

    assert(text->text_attr->fontmask >= 0 && text->text_attr->fontmask < N_FONTS);
    f = PSFE_MaskToFI[text->text_attr->fontmask];
    assert(f != NULL);
    /*
    ** Font info is scale by 1000, I want everything to be in points*10,
    ** so the scale factor is 1000/10 =>100
    */
    sf = scale_factor(cx, text->text_attr->size, text->text_attr->fontmask) / 100.0;

    text_info->ascent = (f->fontBBox.ury * sf);
    text_info->descent = -(f->fontBBox.lly * sf);

    width = height = left = right = x = y = 0.0;
    cp = ((unsigned char*) text->text)+start;

    if (cx->prSetup->otherFontName) {
	measure_non_latin1_text(cx, f, cp, start, last, sf, text_info);
	return;
    }


    for (i = start; i <= last; cp++, i++) {
	PS_CharInfo *temp;

	temp = f->chars + *cp;
	width = max(width, x + temp->wx);
	height = max(height, y + temp->charBBox.ury);
	left = min(left, x + temp->charBBox.llx);
	right = max(right, x + temp->charBBox.urx);
	x += temp->wx;
	y += temp->wy;
    }
    text_info->max_width = width * sf;
    text_info->lbearing = left * sf;
    text_info->rbearing = right * sf;
}

/*
** Here's a lovely hack.  This routine draws the header or footer for a page.
*/

void
xl_annotate_page(MWContext *cx, char *template, int y, int delta_dir, int pn)
{
  float sf;
  int as, dc, ty;
  char left[300], middle[300], right[300], *bp, *fence;

  if (template == NULL)
    return;

  sf = scale_factor(cx, 1, LO_FONT_NORMAL) / 10.0;
  as = PSFE_MaskToFI[LO_FONT_NORMAL]->fontBBox.ury * sf;
  dc = -PSFE_MaskToFI[LO_FONT_NORMAL]->fontBBox.lly * sf;

#if 0
  y += cx->prInfo->page_break;
#endif
  ty = y;
  y += cx->prInfo->page_topy;
  if (delta_dir == 1)
  {
    y += (dc * delta_dir);
#if 0
    ty = y + as + dc;
#endif
  } else {
    y -= (as + dc);
#if 0
    ty = y - (dc + dc);
#endif
  }

  bp = left;
  fence = left + sizeof left - 1;  
  *left = *middle = *right = '\0';
  while (*template != '\0')
  {
    int useit;
    int my_pn;
    
    useit = 1;
    
    my_pn = pn;
    if (*template == '%')
    {
      useit = 0;
      switch (*++template)
      {
      default:
	useit = 1;
	break;
      case '\0':
	break;
      case 'u': case 'U':
      {
	char *up;
	up = cx->prSetup->url->address;
	while (*up != '\0' && bp < fence)
	  *bp++ = *up++;
	break;
      }
      case 't': case 'T':
      {
	char *tp;
	tp = cx->prInfo->doc_title;
	while (*tp != '\0' && bp < fence)
	  *bp++ = *tp++;
	break;
      }
      case 'm': case 'M':
	*bp = '\0';
	bp = middle;
	fence = middle + sizeof middle - 1;
	break;
      case 'r': case 'R':
	*bp = '\0';
	bp = right;
	fence = right + sizeof right - 1;
	break;
      case 's': case 'S':
	xl_moveto(cx, 0, y);
	xl_box(cx, cx->prInfo->page_width, POINT_TO_PAGE(1));
	xl_fill(cx);
	break;
      case 'n': case 'N':
	my_pn = cx->prInfo->n_pages;
      case 'p': case 'P':
      {
  
	char bf[20], *pp;
	sprintf(bf, "%d of %d", my_pn+1, cx->prInfo->n_pages);
	pp = bf;
	while (*pp && bp < fence)
	  *bp++ = *pp++;
	break;
      }	

      case 'd':
      case 'D':
      {
	time_t now;
	struct tm *tp;
	char bf[50], *dp;
	
	now = time(NULL);
	tp = localtime(&now);
	FE_StrfTime(cx, bf, 50, XP_DATE_TIME_FORMAT, tp);

	dp = bf;
	while (*dp && bp < fence)
	  *bp++ = *dp++;
      }
      }
      if (useit == 0 && *template != '\0')
	template++;
    }
    if (useit && bp < fence)
      *bp++ = *template++;
  }
  *bp = '\0';
  
  XP_FilePrintf(cx->prSetup->out, "%d f0 ", scale_factor(cx, 1, LO_FONT_NORMAL));
  if (*left != '\0')
  {
    xl_moveto_loc(cx, cx->prSetup->left/2, ty);
    xl_show(cx, left, strlen(left), "");
  }
  if (*middle != '\0') 
  {
    xl_moveto_loc(cx, cx->prInfo->page_width / 2, ty);
    xl_show(cx, middle, strlen(middle), "c");
  }
  if (*right != '\0')
  {
    xl_moveto_loc(cx, cx->prInfo->page_width+cx->prSetup->left+cx->prSetup->right/2, ty);
    xl_show(cx, right, strlen(right), "r");
  }
}

MODULE_PRIVATE int
PSFE_GetTextInfo(MWContext *cx, LO_TextStruct *text, LO_TextInfo *text_info)
{
    ps_measure_text(cx, text, text_info, 0, text->text_len-1);
    return 0;
}

MODULE_PRIVATE void
PSFE_LayoutNewDocument(MWContext *cx, URL_Struct *url, int32 *w, int32 *h, int32 *mw, int32* mh)
{
    *w = cx->prInfo->page_width;
    *h = cx->prInfo->page_height;
    *mw = *mh = 0;
}

MODULE_PRIVATE void
PSFE_FinishedLayout(MWContext *cx)
{
}

static void
display_non_latin1_multibyte_text(MWContext *cx, unsigned char *str, int len, int sf,
	int fontmask)
{
	int		charSize;
	int		convert;
	XP_File		f;
	unsigned char	*freeThis;
	CCCDataObject	obj;
	unsigned char	*out;
	int		outlen;
	unsigned char	*start;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cx);
	int16 win_csid = INTL_GetCSIWinCSID(csi);

	obj = INTL_CreateCharCodeConverter();
	if (!obj)
	{
		return;
	}
	if (win_csid != cx->prSetup->otherFontCharSetID)
	{
		convert = INTL_GetCharCodeConverter(win_csid,
			cx->prSetup->otherFontCharSetID, obj);
	}
	else
	{
		convert = 0;
	}

	f = cx->prSetup->out;
	freeThis = NULL;

	while (len > 0)
	{
		if ((*str) & 0x80)
		{
			start = str;
			while ((len > 0) && ((*str) & 0x80))
			{
				charSize = INTL_CharLen(win_csid, str);
				len -= charSize;
				str += charSize;
			}
			XP_FilePrintf(f, "%d of\n", sf);
			if (convert)
			{
				out = INTL_CallCharCodeConverter(obj, start,
					str - start);
				outlen = strlen(out);
				freeThis = out;
			}
			else
			{
				out = start;
				outlen = str - start;
			}
			XP_FilePrintf(f, "<");
			while (outlen > 0)
			{
				XP_FilePrintf(f, "%02x", *out);
				outlen--;
				out++;
			}
			XP_FilePrintf(f, "> show\n");
			if (freeThis)
			{
				free(freeThis);
				freeThis = NULL;
			}
		}
		else
		{
			start = str;
			while ((len > 0) && (!((*str) & 0x80)))
			{
				len--;
				str++;
			}
			XP_FilePrintf(f, "%d f%d\n", sf, fontmask);
			xl_show(cx, (char *) start, str - start, "");
		}
	}

	INTL_DestroyCharCodeConverter(obj);
}


static void
display_non_latin1_singlebyte_text(MWContext *cx, unsigned char *str, int len, int sf,
	int fontmask)
{
	int		charSize;
	int		convert;
	XP_File		f;
	unsigned char	*freeThis;
	CCCDataObject	obj;
	unsigned char	*out;
	int		outlen;
	unsigned char	*start;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cx);
	int16 win_csid = INTL_GetCSIWinCSID(csi);

	obj = INTL_CreateCharCodeConverter();
	if (!obj)
	{
		return;
	}
	if (win_csid != cx->prSetup->otherFontCharSetID)
	{
		convert = INTL_GetCharCodeConverter(win_csid,
			cx->prSetup->otherFontCharSetID, obj);
	}
	else
	{
		convert = 0;
	}

	f = cx->prSetup->out;
	freeThis = NULL;
	while (len > 0)
	{
	    if ((*str) & 0x80)
	    {
		start = str;
		while ((len > 0) && ((*str) & 0x80))
		{
			charSize = INTL_CharLen(win_csid, str);
			len -= charSize;
			str += charSize;
		}
			XP_FilePrintf(f, "%d of\n", sf);
			if (convert)
			{
				out = INTL_CallCharCodeConverter(obj, start,
					str - start);
				outlen = strlen(out);
				freeThis = out;
			}
			else
			{
				out = start;
				outlen = str - start;
			}
			XP_FilePrintf(f, "<");
			while (outlen > 0)
			{
				XP_FilePrintf(f, "%02x", *out);
				outlen--;
				out++;
			}
			XP_FilePrintf(f, "> show\n");
			
			if (freeThis)
			{
				free(freeThis);
				freeThis = NULL;
			}
	    }
	    else
	    {
			start = str;
			while ((len > 0) && (!((*str) & 0x80)))
			{
				len--;
				str++;
			}
			XP_FilePrintf(f, "%d of\n", sf);
			xl_show(cx, (char *) start, str - start, "");
	    }
	}
	INTL_DestroyCharCodeConverter(obj);
}

MODULE_PRIVATE void
PSFE_DisplaySubtext(MWContext *cx, int iLocation, LO_TextStruct *text,
		    int32 start_pos, int32 end_pos, XP_Bool notused)
{
  int x, y, x2, top, height;
  LO_TextInfo ti;
  int sf;
  INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cx);
  int16 win_csid = INTL_GetCSIWinCSID(csi);

  x = text->x + text->x_offset;
  y = text->y + text->y_offset;
  top = y;
  height = text->height;
  ps_measure_text(cx, text, &ti, 0, start_pos-1);
  y += ti.ascent;		/* Move to baseline */
  x += ti.max_width;		/* Skip over un-displayed text */
  if (!XP_CheckElementSpan(cx, top, height))
    return;

  sf = scale_factor(cx, text->text_attr->size, text->text_attr->fontmask);
  xl_moveto(cx, x, y);
  if (cx->prSetup->otherFontName) {
       	if (win_csid & MULTIBYTE) 
    	     display_non_latin1_multibyte_text(cx, 
    				((unsigned char *) (text->text)) + start_pos,
			    	end_pos - start_pos + 1, sf,
			    	(int) text->text_attr->fontmask);
	else display_non_latin1_singlebyte_text(cx,
				((unsigned char *) (text->text)) + start_pos,
			    	end_pos - start_pos + 1, sf,
			    	(int) text->text_attr->fontmask);
    	return;
  }
  XP_FilePrintf(cx->prSetup->out, "%d f%d\n", sf,
		(int) text->text_attr->fontmask);
  xl_show(cx, ((char*) text->text) + start_pos, end_pos - start_pos + 1,"");

  /* Underline the text? */

  if (text->text_attr->attrmask & LO_ATTR_UNDERLINE) {
	  y = text->y + text->y_offset + ti.ascent + ti.descent/2;
	  x2 = x + text->width - 1;
	  xl_line(cx, x, y, x2, y, THIN_LINE_THICKNESS);
  }

  /* Strikeout? */

  if (text->text_attr->attrmask & LO_ATTR_STRIKEOUT) {
	  y = text->y + text->y_offset + height/2;
	  x2 = x + text->width - 1;
	  xl_line(cx, x, y, x2, y, THIN_LINE_THICKNESS);
  }

}

MODULE_PRIVATE void
PSFE_DisplayText(MWContext *cx, int iLocation, LO_TextStruct *text, XP_Bool b)
{
    (cx->funcs->DisplaySubtext)(cx, iLocation, text, 0, text->text_len-1, b);
}

/*
** Called at the end of each "Line" which could be a tiny line-section
** inside of a table cell or subdoc.
*/
MODULE_PRIVATE void
PSFE_DisplayLineFeed(MWContext *cx, int iLocation, LO_LinefeedStruct *line_feed,
		     XP_Bool b)
{
#ifdef NOT_THIS_TIME
    if (cx->prInfo->layout_phase) {
	if (cx->prInfo->in_pre) {
	    if (cx->prInfo->pre_start == -1)
		cx->prInfo->pre_start = line_feed->y;
	    cx->prInfo->pre_end = line_feed->y+line_feed->line_height;
	}
	emit_y(cx, line_feed->y, line_feed->y+line_feed->line_height);
    } else if (cx->prInfo->interesting != NULL) {
	InterestingPRE *p;
	p = (InterestingPRE *) XP_ListPeekTopObject(cx->prInfo->interesting);
	if (p == NULL) {
	    XP_DELETE(cx->prInfo->interesting);
	    cx->prInfo->interesting = NULL;
	} else if (line_feed->y + line_feed->line_height >= p->end) {
	    (void) XP_ListRemoveTopObject(cx->prInfo->interesting);
	    XP_DELETE(p);
	}
    }
#endif
}

MODULE_PRIVATE void
PSFE_DisplayHR(MWContext *cx, int iLocation , LO_HorizRuleStruct *HR)
{
  int delta;
  int top, bottom, height;

  top = HR->y + HR->y_offset;
  height = HR->height;
  if (!XP_CheckElementSpan(cx, top, height))
    return;
  delta = (height - height*cx->prSetup->rules) / 2;
  top += delta;
  bottom = top + height*cx->prSetup->rules;

  xl_moveto(cx, HR->x+HR->x_offset, top);
  xl_box(cx, HR->width, height*cx->prSetup->rules);
  xl_fill(cx);
}

MODULE_PRIVATE void
PSFE_DisplayBullet(MWContext *cx, int iLocation, LO_BullettStruct *bullet)
{
  int top, height;

  top = bullet->y + bullet->y_offset;
  height = bullet->height;
  if (!XP_CheckElementSpan(cx, top, height))
    return;

  switch (bullet->bullet_type) {
    case BULLET_ROUND:
      xl_moveto(cx,
		 bullet->x+bullet->x_offset+height/2,
		 top+height/2);
      xl_circle(cx, bullet->width, height);
      xl_stroke(cx);
      break;
    case BULLET_BASIC:
      xl_moveto(cx,
		 bullet->x+bullet->x_offset+height/2,
		 top+height/2);
      xl_circle(cx, bullet->width, height);
      xl_fill(cx);
      break;
    case BULLET_MQUOTE:
    case BULLET_SQUARE:
      xl_moveto(cx, bullet->x+bullet->x_offset, top);
      xl_box(cx, bullet->width, height);
      xl_fill(cx);
      break;
#ifdef DEBUG
      default:
	  XP_Trace("Strange bullet type %d", bullet->bullet_type);
#endif
  }
}

MODULE_PRIVATE void
PSFE_DisplayBorder(MWContext *context, int iLocation, int x, int y, int width,
                   int height, int bw, LO_Color *color, LO_LineStyle style)
{
    /* XXXM12N Implement me. */
}

MODULE_PRIVATE void
PSFE_DisplayFeedback(MWContext *context, int iLocation, LO_Element *element)
{
}

PUBLIC void PSFE_AllConnectionsComplete(MWContext *cx) 
{
      /*
      ** All done, time to let everything go.
      */
  if (cx->prSetup->status >= 0)
    xl_generate_postscript(cx);
  (*cx->prSetup->completion)(cx->prSetup);
  LO_DiscardDocument(cx);
  xl_finalize_translation(cx);
  XP_FREE(cx->prInfo->doc_title);
  XP_CleanupPrintInfo(cx);
  XP_DELETE(cx->funcs);

  /* Release the color space. */
  if (cx->color_space) {
      IL_ReleaseColorSpace(cx->color_space);
      cx->color_space = NULL;
  }

  /* Destroy the image group context. */
  IL_DestroyGroupContext(cx->img_cx);
  cx->img_cx = NULL;

  XP_DELETE(cx);
}


/* The code below provides very basic support for printing the current text
   associated with form elements in the absence of a comprehensive solution
   for 4.0.  The code does not display the form elements themselves, and
   the form element margins still need to be tweaked to get the alignment
   right.  If you found the time to read this, then you are the right person
   to fix the bugs in this code. */
#define FORM_LEFT_MARGIN       0
#define FORM_RIGHT_MARGIN      0
#define FORM_TOP_MARGIN        10
#define FORM_BOTTOM_MARGIN     10

#define FORM_BOX_THICKNESS     12
#define FORM_BOX_LEFT_MARGIN   24
#define FORM_BOX_RIGHT_MARGIN  24
#define FORM_BOX_TOP_MARGIN    10
#define FORM_BOX_BOTTOM_MARGIN 10

#define RADIOBOX_THICKNESS     20
#define RADIOBOX_WIDTH         100
#define RADIOBOX_HEIGHT        100
#define RADIOBOX_LEFT_MARGIN   0
#define RADIOBOX_RIGHT_MARGIN  20

#define CHECKBOX_THICKNESS     20
#define CHECKBOX_WIDTH         100
#define CHECKBOX_HEIGHT        100
#define CHECKBOX_LEFT_MARGIN   0
#define CHECKBOX_RIGHT_MARGIN  20

#define SCROLLBAR_THICKNESS         12
#define SCROLLBAR_ARROW_WIDTH       66
#define SCROLLBAR_ARROW_HEIGHT      66
#define SCROLLBAR_ARROW_THICKNESS   12
#define SCROLLBAR_SLIDER_MARGIN     6
#define SCROLLBAR_SLIDER_THICKNESS  12
#define SCROLLBAR_MIN_SLIDER_LENGTH 36
#define SCROLLBAR_MIN_HEIGHT        (2*SCROLLBAR_THICKNESS+2*SCROLLBAR_ARROW_HEIGHT+2*SCROLLBAR_SLIDER_MARGIN+SCROLLBAR_MIN_SLIDER_LENGTH)
#define SCROLLBAR_WIDTH             (2*SCROLLBAR_THICKNESS+SCROLLBAR_ARROW_WIDTH)

#define COMBOBOX_ARROW_THICKNESS    16
#define COMBOBOX_ARROW_WIDTH        80
#define COMBOBOX_ARROW_MARGIN       24

typedef struct {
    int size;
    LO_TextStruct **text;
} TextArray;

 /* Determine length of string, excluding any blank space at the end. */
static int
reduce_length(char *str) 
{
    int i;

    i = XP_STRLEN(str) - 1;
    while (str[i] == ' ')
        i--;
	str[i+1] = '\0';
    return (i + 1);
}

/* Free the text array. */
static void
free_text_array(TextArray *text_array)
{
    int i;
    
    for (i = 0; i < text_array->size; i++) {
        if (text_array->text[i]) {
            XP_FREE(text_array->text[i]);
            text_array->text[i] = NULL;
        }
    }
    text_array->size = 0;
    XP_FREE(text_array->text);
    XP_FREE(text_array);
}

/* Returns the next line pointed to by ptr. Each line should have maximum cols characters */
static char *
get_next_line(char **pptr, int cols) 
{
	char *cr = NULL;
	char *ptr = *pptr;
	char *line = NULL;
	char *bptr = NULL;
	int   len = 0;

	if (*ptr == '\0') {
		line = XP_CALLOC(1, sizeof(char));
		line[0] = '\0';
		return line;
	}

	/* check if there is a newline character */

	cr = XP_STRCHR(ptr, '\n');
	if (cr) {
		/* line length is less than max cols */
		len = (int)(cr - ptr);
		if (len <= cols) {
			line = XP_CALLOC(len+1, sizeof(char));
			XP_MEMCPY(line, ptr, len);
			line[len] = '\0';
			*pptr = cr+1;
			return line;
		}
	}

	/* if there is no new line character, see if buffer length
	 * is less than cols 
	 */

	len = XP_STRLEN(ptr);
	if (len <= cols) {
		line = XP_CALLOC(len+1, sizeof(char));
		XP_MEMCPY(line, ptr, len);
		line[len] = '\0';
		*pptr = ptr + len;
		return line;
	}
	
	/* now we have to get to the delimiter before the max length */
	
	bptr = ptr + cols - 1;
	while (bptr != ptr) {
		if ((*bptr == ' ') || (*bptr == '\t')) {
			break;
		}
		bptr--;
	}

	if (bptr == ptr) {
		line = XP_CALLOC(1, sizeof(char));
		line[0] = '\0';
		return line;
	}
	else {
		len = (int)(bptr - ptr + 1);
		line = XP_CALLOC(len+1, sizeof(char));
		XP_MEMCPY(line, ptr, len);
		line[len] = '\0';
		*pptr = ptr + len;
		return line;
	}
}

/* Make a temporary text element to be used to display form elements which
   have a text field.  */
static TextArray *
make_text_array(MWContext *cx, LO_FormElementStruct *form)
{
    LO_TextStruct *text;
    TextArray *text_array;
    
    if (form->element_data == NULL)
        return NULL;

    text_array = XP_NEW_ZAP(TextArray);
    if(!text_array)
        return NULL;

    switch (form->element_data->type) {
    case FORM_TYPE_FILE:
    case FORM_TYPE_TEXT:
    case FORM_TYPE_PASSWORD:
    case FORM_TYPE_SUBMIT:
    case FORM_TYPE_RESET:
    case FORM_TYPE_BUTTON:
    case FORM_TYPE_SELECT_ONE:
        text_array->size = 1;
        text_array->text =
            (LO_TextStruct**)XP_CALLOC(1, sizeof(LO_TextStruct*));
        if (!text_array->text) {
            free_text_array(text_array);
            return NULL;
        }

        text = XP_NEW_ZAP(LO_TextStruct);
        if (!text) {
            free_text_array(text_array);
            return NULL;
        }

        switch (form->element_data->type) {
        case FORM_TYPE_FILE:
        case FORM_TYPE_TEXT:
        case FORM_TYPE_PASSWORD:
            text->text = form->element_data->ele_text.current_text;
            if (!text->text)
                text->text = form->element_data->ele_text.default_text;
            break;
            
        case FORM_TYPE_SUBMIT:
        case FORM_TYPE_RESET:
        case FORM_TYPE_BUTTON:
            text->text = form->element_data->ele_minimal.value;
            break;

		case FORM_TYPE_SELECT_ONE:
			if (form->element_data->ele_select.option_cnt > 0) {

				int option_cnt = form->element_data->ele_select.option_cnt;
				lo_FormElementOptionData* options =
					(lo_FormElementOptionData*)
					form->element_data->ele_select.options;
				int selected = -1;
				int def_selected = -1;
				int i;

				for (i = 0; i < option_cnt; i++) {
					if (options[i].selected) {
						selected = i;
						break;
					} else if (options[i].def_selected) {
						def_selected = i;
					}
				}
				
				if (selected == -1) {
				   if (def_selected != -1)
					   selected = def_selected;
				   else
					   selected = 0;
				}
				
				text->text = options[selected].text_value;
			}
		    break;

        default:
            XP_ASSERT(0);
            break;
        }
        
        if (!text->text) {
            free_text_array(text_array);
            XP_FREE(text);
            return NULL;
        }
        text->text_len = reduce_length((char*)text->text);
        text->text_attr = form->text_attr;

        text_array->text[0] = text;
        break;
    
	case FORM_TYPE_TEXTAREA:
		{
			char *string = (char *)form->element_data->ele_textarea.current_text;
			int columns = form->element_data->ele_textarea.cols;
			int rows = form->element_data->ele_textarea.rows;
			char *string_ptr;
			int i;

            if (!string)
                string = (char *)form->element_data->ele_textarea.default_text;

			text_array->size = rows;
			text_array->text = (LO_TextStruct**)XP_CALLOC(text_array->size,
														  sizeof(LO_TextStruct*));
			if (!text_array->text) {
				free_text_array(text_array);
				return NULL;
			}

			for (string_ptr = &string[0], i = 0; i < text_array->size; i++) {

				char *line = get_next_line(&string_ptr, columns);

				text = XP_NEW_ZAP(LO_TextStruct);
				if (!text) {
					free_text_array(text_array);
					return NULL;
				}

				text->text = (PA_Block)line;
				if (!text->text) {
					free_text_array(text_array);
					XP_FREE(text);
					return NULL;
				}

				text->text_len = reduce_length((char*)text->text);
				text->text_attr = form->text_attr;

				text_array->text[i] = text;
			}
		}
        break;

    case FORM_TYPE_SELECT_MULT:
    {
        int i;
        lo_FormElementOptionData *options;
		int size = form->element_data->ele_select.size;
		int option_cnt = form->element_data->ele_select.option_cnt;

		/*
		 *    Try to use size, unless it's bogus or less than option_cnt
		 *    There is a small bug here: when a multi list has a size
		 *    greater than the number of elements, we won't show the
		 *    blanks at the end of the list, the display FEs do.
		 */
		if (size < 1 || size > option_cnt)
			size = option_cnt;

		if (size < 1) { /* paranoia: just in case option_cnt is bogus */
            free_text_array(text_array);
            return NULL;
        }

        text_array->size = size;
        text_array->text = (LO_TextStruct**)XP_CALLOC(text_array->size,
													  sizeof(LO_TextStruct*));
        if (!text_array->text) {
            free_text_array(text_array);
            return NULL;
        }

        options =
            (lo_FormElementOptionData *)form->element_data->ele_select.options;

        for (i = 0; i < text_array->size; i++) {

            text = XP_NEW_ZAP(LO_TextStruct);
            if (!text) {
                free_text_array(text_array);
                return NULL;
            }

            text->text = options[i].text_value;
            if (!text->text) {
                free_text_array(text_array);
                XP_FREE(text);
                return NULL;
            }

            text->text_len = reduce_length((char*)text->text);
            text->text_attr = form->text_attr;

            text_array->text[i] = text;
        }
    }
    break;
        
    default:
        break;
    }

    return text_array;
}

MODULE_PRIVATE void
PSFE_GetFormElementInfo(MWContext *cx, LO_FormElementStruct *form)
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cx);
	int16 win_csid = INTL_GetCSIWinCSID(csi);

    if (form->element_data == NULL)
        return;

    switch (form->element_data->type) {

    case FORM_TYPE_FILE:
    case FORM_TYPE_TEXT:
    case FORM_TYPE_PASSWORD:
    case FORM_TYPE_TEXTAREA:
    case FORM_TYPE_SELECT_ONE:
    case FORM_TYPE_SELECT_MULT:
    case FORM_TYPE_SUBMIT:
    case FORM_TYPE_RESET:
    case FORM_TYPE_BUTTON:
    {
        int i, width, height;
        LO_TextInfo text_info;
        TextArray *text_array;
		int descent = 0;
		int bottom_margin = 0;
		int shadow_thickness = 0;

        text_array = make_text_array(cx, form);
        if (!text_array)
            return;

        width = 0;
        height = 0;
        for (i = 0; i < text_array->size; i++) {
            PSFE_GetTextInfo(cx, text_array->text[i], &text_info);

			if ((form->element_data->type == FORM_TYPE_TEXT) ||
				(form->element_data->type == FORM_TYPE_PASSWORD) ||
				(form->element_data->type == FORM_TYPE_TEXTAREA)) {

				/* For empty text fields, we should calculate width using 
				 * form->element_data->ele_text.size
				 * For non-empty text fields, we should make sure that the
				 * bonding box is bigger than the text
				 */
				
				LO_TextInfo    temp_info;
				TextArray     *temp_text_array = NULL;
				LO_TextStruct *text = NULL;
				int            columns = 0;

				temp_text_array = XP_NEW_ZAP(TextArray);
				temp_text_array->size = 1;
				temp_text_array->text = (LO_TextStruct**)XP_CALLOC(1, sizeof(LO_TextStruct*));
				text = XP_NEW_ZAP(LO_TextStruct);
				text->text = (PA_Block)" ";
				text->text_len = 1;
				text->text_attr = text_array->text[i]->text_attr;
				temp_text_array->text[0] = text;

				PSFE_GetTextInfo(cx, temp_text_array->text[0], &temp_info);				
				if (form->element_data->type == FORM_TYPE_TEXTAREA) {
					lo_FormElementTextareaData *data = &form->element_data->ele_textarea;
					columns = ((win_csid & MULTIBYTE) ? (data->cols + 1) / 2 : data->cols);
					text_info.max_width =
						MAX(text_info.max_width, temp_info.max_width * columns);
				}
				else {
					lo_FormElementTextData *data = &form->element_data->ele_text;
					columns = ((win_csid & MULTIBYTE) ? (data->size + 1) / 2 : data->size);
					text_info.max_width =
						MAX(text_info.max_width, temp_info.max_width * columns);
				}

				free_text_array(temp_text_array);
			}

            width = MAX(width, text_info.max_width +
                        FORM_LEFT_MARGIN + FORM_RIGHT_MARGIN);
            height = MAX(height, text_info.ascent + text_info.descent +
                         FORM_TOP_MARGIN + FORM_BOTTOM_MARGIN);
        }

		/* adjust height */

		if (form->element_data->type == FORM_TYPE_TEXTAREA)
			height *= form->element_data->ele_textarea.rows;
		else
			height *= text_array->size;

		bottom_margin = FORM_BOTTOM_MARGIN;
		descent = text_info.descent;

		if (form->element_data->type == FORM_TYPE_SELECT_ONE) {
			/* leave space for bonding box and down arrow */
			height = height + 2 * FORM_BOX_THICKNESS +
				FORM_BOX_TOP_MARGIN + FORM_BOX_BOTTOM_MARGIN;
			width = width + 2 * FORM_BOX_THICKNESS + 
				FORM_BOX_LEFT_MARGIN + FORM_BOX_RIGHT_MARGIN + 
				1 * COMBOBOX_ARROW_MARGIN + COMBOBOX_ARROW_WIDTH;
			shadow_thickness += FORM_BOX_THICKNESS;
			bottom_margin += FORM_BOX_BOTTOM_MARGIN;
		}

		if ((form->element_data->type == FORM_TYPE_SUBMIT) ||
			(form->element_data->type == FORM_TYPE_RESET) ||
			(form->element_data->type == FORM_TYPE_BUTTON) ||
			(form->element_data->type == FORM_TYPE_TEXT) ||
			(form->element_data->type == FORM_TYPE_TEXTAREA) ||
			(form->element_data->type == FORM_TYPE_PASSWORD) ||
			(form->element_data->type == FORM_TYPE_SELECT_MULT)) {
			/* leave space for bonding box */
			width = width + 2 * FORM_BOX_THICKNESS + 
				FORM_BOX_LEFT_MARGIN + FORM_BOX_RIGHT_MARGIN;
			height = height + 2 * FORM_BOX_THICKNESS +
				FORM_BOX_TOP_MARGIN + FORM_BOX_BOTTOM_MARGIN;;

			shadow_thickness += FORM_BOX_THICKNESS;
			bottom_margin += FORM_BOX_BOTTOM_MARGIN;
		}

		/* leave space for scrollbar */
		if ((form->element_data->type == FORM_TYPE_SELECT_MULT) &&
			(height >= SCROLLBAR_MIN_HEIGHT)) {
			/* no need to provide scrollbar if the list/text is not long enough */
			width += SCROLLBAR_WIDTH;
		}

        if (!form->width)
            form->width = width;
        if (!form->height)
            form->height = height;

		/* readjust the baseline for the form element */
		form->baseline = form->height - (descent + bottom_margin + shadow_thickness);

        free_text_array(text_array);
    }
    break;
    
    case FORM_TYPE_RADIO:
	{
		form->width = RADIOBOX_WIDTH + RADIOBOX_LEFT_MARGIN + RADIOBOX_RIGHT_MARGIN;
		form->height = RADIOBOX_HEIGHT;
	}
	break;

    case FORM_TYPE_CHECKBOX:
	{
		form->width = CHECKBOX_WIDTH + CHECKBOX_LEFT_MARGIN + CHECKBOX_RIGHT_MARGIN;
		form->height = CHECKBOX_HEIGHT;
	}
	break;

    default:
        /* Other types of form element are unsupported. */
        XP_TRACE(("PSFE_GetFormElementInfo: unsupported form element type."));
        break;
        
    }
}

MODULE_PRIVATE void
PSFE_DisplayFormElement(MWContext *cx, int loc, LO_FormElementStruct *form)
{
    if (form->element_data == NULL)
        return;
    switch (form->element_data->type) {
    case FORM_TYPE_FILE:
    case FORM_TYPE_TEXT:
    case FORM_TYPE_PASSWORD:
    case FORM_TYPE_SELECT_MULT:
    case FORM_TYPE_TEXTAREA:
    case FORM_TYPE_SUBMIT:
    case FORM_TYPE_RESET:
    case FORM_TYPE_BUTTON:
    {
        int i, x, y, text_width, text_height, delta_height;
        TextArray *text_array;
        LO_TextStruct *text;
        
        text_array = make_text_array(cx, form);
        if (!text_array)
            return;

        x = form->x + form->x_offset + FORM_LEFT_MARGIN;
        y = form->y + form->y_offset + FORM_TOP_MARGIN;

		if (!XP_CheckElementSpan(cx, y, form->height))
			return;

		/* draw scrollbar if necessary */

		if ((form->element_data->type == FORM_TYPE_SELECT_MULT) &&
			(form->height >= SCROLLBAR_MIN_HEIGHT)) {
			/* no need to provide scrollbar if the list/text is not long enough */

			int scrollbar_x, scrollbar_y;
			int arrow_x, arrow_y;

			scrollbar_x = form->x + form->width - SCROLLBAR_WIDTH;
			scrollbar_y = form->y + form->y_offset + FORM_TOP_MARGIN;

			xl_draw_3d_border(cx, scrollbar_x, scrollbar_y,
							  SCROLLBAR_WIDTH,
							  form->height-FORM_TOP_MARGIN-FORM_BOTTOM_MARGIN, 
							  SCROLLBAR_THICKNESS, 
							  DARK_GREY, LIGHT_GREY);

			form->width -= SCROLLBAR_WIDTH;

			/* draw arrows */

			arrow_x = scrollbar_x + SCROLLBAR_THICKNESS;
			arrow_y = scrollbar_y + SCROLLBAR_THICKNESS;

			xl_draw_3d_arrow(cx, 
							 arrow_x, arrow_y,
							 SCROLLBAR_ARROW_THICKNESS,
							 SCROLLBAR_ARROW_WIDTH,
							 SCROLLBAR_ARROW_HEIGHT,
							 TRUE, /* up arrow */
							 LIGHT_GREY, /* left */
							 DARK_GREY,  /* right */
							 DARK_GREY   /* base */);

			arrow_x = scrollbar_x + SCROLLBAR_THICKNESS;
			arrow_y = form->y + form->y_offset + form->height - FORM_BOTTOM_MARGIN-
				SCROLLBAR_ARROW_THICKNESS - SCROLLBAR_ARROW_HEIGHT;

			xl_draw_3d_arrow(cx, 
							 arrow_x, arrow_y,
							 SCROLLBAR_ARROW_THICKNESS,
							 SCROLLBAR_ARROW_WIDTH,
							 SCROLLBAR_ARROW_HEIGHT,
							 FALSE, /* down arrow */
							 LIGHT_GREY, /* left */
							 DARK_GREY,  /* right */
							 LIGHT_GREY   /* base */);

			/* draw slider */

		}

		/* if there is a bonding box, create the bonding box first */

		if ((form->element_data->type == FORM_TYPE_SUBMIT) ||
			(form->element_data->type == FORM_TYPE_RESET) ||
			(form->element_data->type == FORM_TYPE_BUTTON) ||
			(form->element_data->type == FORM_TYPE_TEXT) ||
			(form->element_data->type == FORM_TYPE_TEXTAREA) ||
			(form->element_data->type == FORM_TYPE_PASSWORD) ||
			(form->element_data->type == FORM_TYPE_SELECT_MULT)) {

			XP_Bool stick_out = FALSE;
			if ((form->element_data->type == FORM_TYPE_SUBMIT) ||
				(form->element_data->type == FORM_TYPE_RESET) ||
				(form->element_data->type == FORM_TYPE_BUTTON))
				stick_out = TRUE;
				
			xl_draw_3d_border(cx, x, y,
							  form->width-FORM_LEFT_MARGIN-FORM_RIGHT_MARGIN,
							  form->height-FORM_TOP_MARGIN-FORM_BOTTOM_MARGIN, 
							  FORM_BOX_THICKNESS, 
							  stick_out ? LIGHT_GREY : DARK_GREY,
							  stick_out ? DARK_GREY : LIGHT_GREY);

			/* readjust the origin and dimensions */

			form->width = form->width - 2 * FORM_BOX_THICKNESS - 
				FORM_BOX_LEFT_MARGIN - FORM_BOX_RIGHT_MARGIN;
			form->height = form->height - 2 * FORM_BOX_THICKNESS -
				FORM_BOX_TOP_MARGIN - FORM_BOX_BOTTOM_MARGIN;
			x = x + FORM_BOX_THICKNESS + FORM_BOX_LEFT_MARGIN;
			y = y + FORM_BOX_THICKNESS + FORM_BOX_TOP_MARGIN;
		}

        delta_height = form->height / text_array->size;
        text_height = delta_height - (FORM_TOP_MARGIN + FORM_BOTTOM_MARGIN);
        text_width = form->width;
        
        for (i = 0; i < text_array->size; i++) {
            PA_Block text_save;

            text = text_array->text[i];
            text->x = x;
            text->y = y;
            text->width = text_width;
            text->height = text_height;

            /* Avoid revealing users' passwords */
            if (form->element_data->type == FORM_TYPE_PASSWORD) {
              char *ptr;

              text_save = text->text;
              text->text = XP_ALLOC_BLOCK(text->text_len + 1);
              ptr = (char *)text->text;
              for (i = 0; i < text->text_len; i++) {
                *ptr++ = '*';
              }
              *ptr = '\0';
            }

            PSFE_DisplayText(cx, FE_VIEW, text, FALSE);
            y += delta_height;

            /* Restore password to readable form */
            if (form->element_data->type == FORM_TYPE_PASSWORD) {
              XP_FREE_BLOCK(text->text);
              text->text = text_save;
            }
        }
        
        free_text_array(text_array);
    }
    break;

    case FORM_TYPE_SELECT_ONE:
    {
        int i, x, y, text_width, text_height, delta_height;
        TextArray *text_array;
        LO_TextStruct *text;
		int arrow_x, arrow_y;
		int arrow_height;
        
        text_array = make_text_array(cx, form);
        if (!text_array)
            return;

        x = form->x + form->x_offset + FORM_LEFT_MARGIN;
        y = form->y + form->y_offset + FORM_TOP_MARGIN;

		if (!XP_CheckElementSpan(cx, y, form->height))
			return;

		/* create the bonding box first */

		xl_draw_3d_border(cx, x, y,
						  form->width-FORM_LEFT_MARGIN-FORM_RIGHT_MARGIN,
						  form->height-FORM_TOP_MARGIN-FORM_BOTTOM_MARGIN, 
						  FORM_BOX_THICKNESS, 
						  LIGHT_GREY, DARK_GREY);

		/* draw arrows */

		arrow_x = form->x + form->x_offset + form->width - FORM_RIGHT_MARGIN -
			COMBOBOX_ARROW_MARGIN - FORM_BOX_THICKNESS - COMBOBOX_ARROW_WIDTH;
		arrow_height = form->height - FORM_TOP_MARGIN - FORM_BOTTOM_MARGIN 
			- 2*FORM_BOX_THICKNESS - FORM_BOX_TOP_MARGIN - FORM_BOX_BOTTOM_MARGIN -
			2 * COMBOBOX_ARROW_MARGIN;
		arrow_y = form->y + form->y_offset + FORM_TOP_MARGIN +
			FORM_BOX_THICKNESS + FORM_BOX_TOP_MARGIN + COMBOBOX_ARROW_MARGIN;
		if (arrow_height < 0) {
			arrow_height = form->height - FORM_TOP_MARGIN - FORM_BOTTOM_MARGIN 
				- 2*FORM_BOX_THICKNESS - FORM_BOX_TOP_MARGIN - FORM_BOX_BOTTOM_MARGIN;
			arrow_y = form->y + form->y_offset + FORM_TOP_MARGIN +
				FORM_BOX_THICKNESS + FORM_BOX_TOP_MARGIN;
		}

		xl_draw_3d_arrow(cx, 
						 arrow_x, arrow_y,
						 COMBOBOX_ARROW_THICKNESS,
						 COMBOBOX_ARROW_WIDTH, arrow_height,
						 FALSE, /* down arrow */
						 LIGHT_GREY, /* left */
						 DARK_GREY,  /* right */
						 LIGHT_GREY   /* base */);

		/* readjust the origin and dimensions */

		form->width = form->width - 3*FORM_BOX_THICKNESS - FORM_BOX_LEFT_MARGIN -
			FORM_BOX_RIGHT_MARGIN - COMBOBOX_ARROW_WIDTH - 2*COMBOBOX_ARROW_MARGIN;
		form->height = form->height - 2 * FORM_BOX_THICKNESS -
			FORM_BOX_TOP_MARGIN - FORM_BOX_BOTTOM_MARGIN;
		x = x + FORM_BOX_THICKNESS + FORM_BOX_LEFT_MARGIN;
		y = y + FORM_BOX_THICKNESS + FORM_BOX_TOP_MARGIN;

        delta_height = form->height / text_array->size;
        text_height = delta_height - (FORM_TOP_MARGIN + FORM_BOTTOM_MARGIN);
        text_width = form->width;
        
        for (i = 0; i < text_array->size; i++) {
            text = text_array->text[i];
            text->x = x;
            text->y = y;
            text->width = text_width;
            text->height = text_height;
            PSFE_DisplayText(cx, FE_VIEW, text, FALSE);
            y += delta_height;
        }
        
        free_text_array(text_array);
    }
    break;

    case FORM_TYPE_RADIO:
	{
	    lo_FormElementToggleData *data = &form->element_data->ele_toggle;
		XP_Bool                   selected = data->toggled;
		int                       x, y;

        x = form->x + form->x_offset + RADIOBOX_LEFT_MARGIN;
        y = form->y + form->y_offset;

		if (!XP_CheckElementSpan(cx, y, form->height))
			return;
		
		xl_draw_3d_radiobox(cx, x, y,
							RADIOBOX_WIDTH, RADIOBOX_HEIGHT, RADIOBOX_THICKNESS,
							selected ? DARK_GREY : LIGHT_GREY,
							selected ? LIGHT_GREY : DARK_GREY,
							selected ? GREY : LIGHTER_GREY);
	}
	break;

    case FORM_TYPE_CHECKBOX:
	{
	    lo_FormElementToggleData *data = &form->element_data->ele_toggle;
		XP_Bool                   selected = data->toggled;
		int                       x, y;

        x = form->x + form->x_offset + CHECKBOX_LEFT_MARGIN;
        y = form->y + form->y_offset;

		if (!XP_CheckElementSpan(cx, y, form->height))
			return;
		
		xl_draw_3d_checkbox(cx, x, y,
							CHECKBOX_WIDTH, CHECKBOX_HEIGHT, CHECKBOX_THICKNESS,
							selected ? DARK_GREY : LIGHT_GREY,
							selected ? LIGHT_GREY : DARK_GREY,
							selected ? GREY : LIGHTER_GREY);
	}
	break;

    default:
        /* Other types of form element are unsupported. */
        XP_TRACE(("PSFE_DisplayFormElement: unsupported form element type."));
        break;
    }
}

MODULE_PRIVATE void
PSFE_SetDocDimension(MWContext *cx, int iloc, int32 iWidth, int32 iLength)
{
  cx->prInfo->doc_width = iWidth;
  cx->prInfo->doc_height = iLength;
}

MODULE_PRIVATE void
PSFE_SetDocTitle(MWContext * cx, char * title)
{
    XP_FREE(cx->prInfo->doc_title);
    cx->prInfo->doc_title = XP_STRDUP(title);
}

MODULE_PRIVATE char *
PSFE_TranslateISOText(MWContext *cx, int charset, char *ISO_Text)
{
    return ISO_Text;
}

MODULE_PRIVATE void
PSFE_BeginPreSection(MWContext* cx)
{
#ifdef LATER
    cx->prInfo->scale = 1.0;
    cx->prInfo->in_pre = TRUE;
    cx->prInfo->pre_start = -1;
#endif
}

MODULE_PRIVATE void
PSFE_EndPreSection(MWContext* cx)
{
#ifdef LATER
    if (cx->prInfo->layout_phase && cx->prInfo->scale != 1.0) {
	InterestingPRE *p = XP_NEW(InterestingPRE);
	p->start = cx->prInfo->pre_start;
	p->end = cx->prInfo->pre_end;
	p->scale = max(cx->prInfo->scale, 0.25);
	if (cx->prInfo->interesting == NULL)
	    cx->prInfo->interesting = XP_ListNew();
	XP_ListAddObjectToEnd(cx->prInfo->interesting, p);
    }
    cx->prInfo->in_pre = FALSE;
#endif
}

void PSFE_DisplayTable(MWContext *cx, int iLoc, LO_TableStruct *table)
{
    int top, height;
    
    top = table->y+table->y_offset;
    height = table->height;
  
    if (table->border_width != 0 && XP_CheckElementSpan(cx, top, height)) {
        xl_draw_border(cx, table->x+table->x_offset, top,
                       table->width, height, table->border_width);
    }
}

void PSFE_DisplayCell(MWContext *cx, int iLoc,LO_CellStruct *cell)
{
    int top, height;
    
    top = cell->y+cell->y_offset;
    height = cell->height;
  
    if (cell->border_width != 0 && XP_CheckElementSpan(cx, top, height)) {
        xl_draw_border(cx, cell->x+cell->x_offset, top,
                       cell->width, height, cell->border_width);
  }
}

MODULE_PRIVATE char *
PSFE_PromptPassword(MWContext *cx, const char *msg)
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


/***************************
 * Plugin printing support *
 ***************************/
#ifndef X_PLUGINS
/* Plugins are disabled */
void PSFE_GetEmbedSize(MWContext *context, LO_EmbedStruct *embed_struct, NET_ReloadMethod force_reload) {}
void PSFE_FreeEmbedElement(MWContext *context, LO_EmbedStruct *embed_struct) {}
void PSFE_CreateEmbedWindow(MWContext *context, NPEmbeddedApp *app) {}
void PSFE_SaveEmbedWindow(MWContext *context, NPEmbeddedApp *app) {}
void PSFE_RestoreEmbedWindow(MWContext *context, NPEmbeddedApp *app) {}
void PSFE_DestroyEmbedWindow(MWContext *context, NPEmbeddedApp *app) {}
void PSFE_DisplayEmbed(MWContext *context, int iLocation, LO_EmbedStruct *embed_struct) {}
#else
void
PSFE_FreeEmbedElement (MWContext *context, LO_EmbedStruct *embed_struct)
{
    NPL_EmbedDelete(context, embed_struct);
    embed_struct->FE_Data = 0;
}

void
PSFE_CreateEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    /* This will be called from NPL_EmbedCreate. Since we don't have
       any widgets that needs to be saved, this is just a stub. */
}

void
PSFE_SaveEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    /* This will be called from NPL_EmbedDelete. Since we don't have
       any widgets that needs to be saved, this is just a stub. */
}

void
PSFE_RestoreEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    /* This will be called from NPL_EmbedCreate. Since we don't have
       any widgets that needs to be saved, this is just a stub. */
}

void
PSFE_DestroyEmbedWindow(MWContext *context, NPEmbeddedApp *App)
{
    /* This will be called from NPL_EmbedDelete and/or
       NPL_FreeSessionData. Since we don't have any widgets that need
       to be destroyed, this is just a stub. */
}

void
PSFE_DisplayEmbed (MWContext *context,
		  int iLocation, LO_EmbedStruct *embed_struct)
{
    NPPrintCallbackStruct npPrintInfo;
    NPEmbeddedApp *eApp;
    NPPrint npprint;

    if (!embed_struct) return;
    eApp = (NPEmbeddedApp *)embed_struct->FE_Data;
    if (!eApp) return;

    npprint.mode = NP_EMBED;
    npprint.print.embedPrint.platformPrint = NULL;
    npprint.print.embedPrint.window.window = NULL;
    npprint.print.embedPrint.window.x =
      embed_struct->x + embed_struct->x_offset;
    npprint.print.embedPrint.window.y =
      embed_struct->y + embed_struct->y_offset;
    npprint.print.embedPrint.window.width = embed_struct->width;
    npprint.print.embedPrint.window.height = embed_struct->height;
    
    npPrintInfo.type = NP_PRINT;
    npPrintInfo.fp = context->prSetup->out;
    npprint.print.embedPrint.platformPrint = (void *) &npPrintInfo;

    NPL_Print(eApp, &npprint);	
}

void
PSFE_GetEmbedSize (MWContext *context, LO_EmbedStruct *embed_struct,
		  NET_ReloadMethod force_reload)
{
  NPEmbeddedApp *eApp = (NPEmbeddedApp *)embed_struct->FE_Data;
  
  if(eApp) return;

  /* attempt to make a plugin */
  if(!(eApp = NPL_EmbedCreate(context, embed_struct)) ||
     (embed_struct->ele_attrmask & LO_ELE_HIDDEN)) {
    return;
  }

  /* Determine if this is a fullpage plugin */
  if ((embed_struct->attribute_cnt > 0) &&
      (!strcmp(embed_struct->attribute_list[0], "src")) &&
      (!strcmp(embed_struct->value_list[0], "internal-external-plugin")))
    {
      embed_struct->width = context->prInfo->page_width;
      embed_struct->height = context->prInfo->page_height;
    }

  embed_struct->FE_Data = (void *)eApp;

  if (NPL_EmbedStart(context, embed_struct, eApp) != NPERR_NO_ERROR) {
    /* Spoil sport! */
    embed_struct->FE_Data = NULL;
    return;
  }
}
#endif /* !X_PLUGINS */

void PSFE_DisplayJavaApp(MWContext *context, int iLocation,
			 LO_JavaAppStruct *java_app)
{
#ifdef JAVA

  int x, y;
  float w, h, tw, th;
  LJAppletData *ad = (LJAppletData *)java_app->session_data;

  if (!XP_CheckElementSpan(context,
			  java_app->x + java_app->x_offset, java_app->height))
    return;

  /* Calculate (x,y) coordinate of bottom left corner. */
  x = java_app->x + java_app->x_offset;
  y = java_app->y + java_app->y_offset + java_app->height;

  w =  PAGE_TO_POINT_F(java_app->width);
  h =  PAGE_TO_POINT_F(java_app->height);

  tw = (float)java_app->width  / context->convertPixX;
  th = (float)java_app->height / context->convertPixY;

  XP_FilePrintf(context->prSetup->out, "BeginEPSF\n");

  /* Clip to the applet's bounding box */
  xl_moveto(context, java_app->x, java_app->y);
  xl_box(context, java_app->width, java_app->height);
  XP_FilePrintf(context->prSetup->out, "clip\n");

  xl_translate(context, x, y);
  XP_FilePrintf(context->prSetup->out, "%g %g scale\n", w/tw, h/th);

  XP_FilePrintf(context->prSetup->out, "%%%%BeginDocument: JavaApplet\n");

  ad->context = context;

  LJ_Applet_print(ad, (void *)context->prSetup->out);

  XP_FilePrintf(context->prSetup->out, "%%%%EndDocument\n");
  XP_FilePrintf(context->prSetup->out, "EndEPSF\n");

#endif /* JAVA */
}

