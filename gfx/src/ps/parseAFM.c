/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * (C) 1988, 1989, 1990 by Adobe Systems Incorporated. All rights reserved.
 *
 * This file may be freely copied and redistributed as long as:
 *   1) This entire notice continues to be included in the file, 
 *   2) If the file has been modified in any way, a notice of such
 *      modification is conspicuously indicated.
 *
 * PostScript, Display PostScript, and Adobe are registered trademarks of
 * Adobe Systems Incorporated.
 * 
 * ************************************************************************
 * THE INFORMATION BELOW IS FURNISHED AS IS, IS SUBJECT TO CHANGE WITHOUT
 * NOTICE, AND SHOULD NOT BE CONSTRUED AS A COMMITMENT BY ADOBE SYSTEMS
 * INCORPORATED. ADOBE SYSTEMS INCORPORATED ASSUMES NO RESPONSIBILITY OR 
 * LIABILITY FOR ANY ERRORS OR INACCURACIES, MAKES NO WARRANTY OF ANY 
 * KIND (EXPRESS, IMPLIED OR STATUTORY) WITH RESPECT TO THIS INFORMATION, 
 * AND EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR PARTICULAR PURPOSES AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * ************************************************************************
 */

/* parseAFM.c
 * 
 * This file is used in conjuction with the parseAFM.h header file.
 * This file contains several procedures that are used to parse AFM
 * files. It is intended to work with an application program that needs
 * font metric information. The program can be used as is by making a
 * procedure call to "parseFile" (passing in the expected parameters)
 * and having it fill in a data structure with the data from the 
 * AFM file, or an application developer may wish to customize this
 * code.
 *
 * There is also a file, parseAFMclient.c, that is a sample application
 * showing how to call the "parseFile" procedure and how to use the data
 * after "parseFile" has returned.
 *
 * Please read the comments in parseAFM.h and parseAFMclient.c.
 *
 * History:
 *	original: DSM  Thu Oct 20 17:39:59 PDT 1988
 *  modified: DSM  Mon Jul  3 14:17:50 PDT 1989
 *    - added 'storageProblem' return code
 *	  - fixed bug of not allocating extra byte for string duplication
 *    - fixed typos
 *  modified: DSM  Tue Apr  3 11:18:34 PDT 1990
 *    - added free(ident) at end of parseFile routine
 *  modified: DSM  Tue Jun 19 10:16:29 PDT 1990
 *    - changed (width == 250) to (width = 250) in initializeArray
 *  modified: liuhai Wed Jun 11 13:53:35 PDT 1997
 *    - customized for Netscape's runtime AFM file parsing
 *    - only 'necessary' parsings are kept, others were thrown away 
 *    - PS_FontInfo is used instead of FontInfo
 */

/*
 *  Mozilla modifications:
 *    - Include xlate_i.h for PS_FontInfo struct definition.
 *    - Minor compiler warning cleanup.
 */

#include "xlate_i.h"
#include <errno.h>
#include <sys/file.h>
#include <math.h>
#include "parseAFM.h"
/*
#include "sunos.h"
*/ 
#define lineterm EOL	/* line terminating character */
#define normalEOF 1	/* return code from parsing routines used only */
			/* in this module */
#define Space "space"   /* used in string comparison to look for the width */
			/* of the space character to init the widths array */
#define False "false"   /* used in string comparison to check the value of */
			/* boolean keys (e.g. IsFixedPitch)  */

#define MATCH(A,B)		(strncmp((A),(B), MAX_NAME) == 0)



/*************************** GLOBALS ***********************/

static char *ident = NULL; /* storage buffer for keywords */


/* "shorts" for fast case statement 
 * The values of each of these enumerated items correspond to an entry in the
 * table of strings defined below. Therefore, if you add a new string as 
 * new keyword into the keyStrings table, you must also add a corresponding
 * parseKey AND it MUST be in the same position!
 *
 * IMPORTANT: since the sorting algorithm is a binary search, the strings of
 * keywords must be placed in lexicographical order, below. [Therefore, the 
 * enumerated items are not necessarily in lexicographical order, depending 
 * on the name chosen. BUT, they must be placed in the same position as the 
 * corresponding key string.] The NOPE shall remain in the last position, 
 * since it does not correspond to any key string, and it is used in the 
 * "recognize" procedure to calculate how many possible keys there are.
 */

static enum parseKey {
  ASCENDER, CHARBBOX, CODE, COMPCHAR, CAPHEIGHT, COMMENT, 
  DESCENDER, ENCODINGSCHEME, ENDCHARMETRICS, ENDCOMPOSITES, 
  ENDFONTMETRICS, ENDKERNDATA, ENDKERNPAIRS, ENDTRACKKERN, 
  FAMILYNAME, FONTBBOX, FONTNAME, FULLNAME, ISFIXEDPITCH, 
  ITALICANGLE, KERNPAIR, KERNPAIRXAMT, LIGATURE, CHARNAME, 
  NOTICE, COMPCHARPIECE, STARTCHARMETRICS, STARTCOMPOSITES, 
  STARTFONTMETRICS, STARTKERNDATA, STARTKERNPAIRS, 
  STARTTRACKKERN, TRACKKERN, UNDERLINEPOSITION, 
  UNDERLINETHICKNESS, VERSION, XYWIDTH, XWIDTH, WEIGHT, XHEIGHT,
  NOPE };

/* keywords for the system:  
 * This a table of all of the current strings that are vaild AFM keys.
 * Each entry can be referenced by the appropriate parseKey value (an
 * enumerated data type defined above). If you add a new keyword here, 
 * a corresponding parseKey MUST be added to the enumerated data type
 * defined above, AND it MUST be added in the same position as the 
 * string is in this table.
 *
 * IMPORTANT: since the sorting algorithm is a binary search, the keywords
 * must be placed in lexicographical order. And, NULL should remain at the
 * end.
 */

static char *keyStrings[] = {
  "Ascender", "B", "C", "CC", "CapHeight", "Comment",
  "Descender", "EncodingScheme", "EndCharMetrics", "EndComposites", 
  "EndFontMetrics", "EndKernData", "EndKernPairs", "EndTrackKern", 
  "FamilyName", "FontBBox", "FontName", "FullName", "IsFixedPitch", 
  "ItalicAngle", "KP", "KPX", "L", "N", 
  "Notice", "PCC", "StartCharMetrics", "StartComposites", 
  "StartFontMetrics", "StartKernData", "StartKernPairs", 
  "StartTrackKern", "TrackKern", "UnderlinePosition", 
  "UnderlineThickness", "Version", "W", "WX", "Weight", "XHeight",
  NULL };
  
/*************************** PARSING ROUTINES **************/ 
  
/*************************** token *************************/

/*  A "AFM File Conventions" tokenizer. That means that it will
 *  return the next token delimited by white space.  See also
 *  the `linetoken' routine, which does a similar thing but 
 *  reads all tokens until the next end-of-line.
 */
 
static char *token(FILE *stream)
{
    int ch, idx;

    /* skip over white space */
    while ((ch = fgetc(stream)) == ' ' || ch == lineterm || 
            ch == ',' || ch == '\t' || ch == ';');
    
    idx = 0;
    while (ch != EOF && ch != ' ' && ch != lineterm 
           && ch != '\t' && ch != ':' && ch != ';') 
    {
        ident[idx++] = ch;
        ch = fgetc(stream);
    } /* while */

    if (ch == EOF && idx < 1) return ((char *)NULL);
    if (idx >= 1 && ch != ':' ) ungetc(ch, stream);
    if (idx < 1 ) ident[idx++] = ch;	/* single-character token */
    ident[idx] = 0;
    
    return(ident);	/* returns pointer to the token */

} /* token */


/*************************** linetoken *************************/

/*  "linetoken" will get read all tokens until the EOL character from
 *  the given stream.  This is used to get any arguments that can be
 *  more than one word (like Comment lines and FullName).
 */

static char *linetoken(FILE *stream)
{
    int ch, idx;

    while ((ch = fgetc(stream)) == ' ' || ch == '\t' ); 
    
    idx = 0;
    while (ch != EOF && ch != lineterm) 
    {
        ident[idx++] = ch;
        ch = fgetc(stream);
    } /* while */
    
    ungetc(ch, stream);
    ident[idx] = 0;

    return(ident);	/* returns pointer to the token */

} /* linetoken */


/*************************** recognize *************************/

/*  This function tries to match a string to a known list of
 *  valid AFM entries (check the keyStrings array above). 
 *  "ident" contains everything from white space through the
 *  next space, tab, or ":" character.
 *
 *  The algorithm is a standard Knuth binary search.
 */

static enum parseKey recognize(register char *ident)
{
    int lower = 0, upper = (int) NOPE, midpoint, cmpvalue;
    BOOL found = FALSE;

    while ((upper >= lower) && !found)
    {
        midpoint = (lower + upper)/2;
        if (keyStrings[midpoint] == NULL) break;
        cmpvalue = strncmp(ident, keyStrings[midpoint], MAX_NAME);
        if (cmpvalue == 0) found = TRUE;
        else if (cmpvalue < 0) upper = midpoint - 1;
        else lower = midpoint + 1;
    } /* while */

    if (found) return (enum parseKey) midpoint;
    else return NOPE;
    
} /* recognize */


/************************* parseGlobals *****************************/

static BOOL parseGlobals(FILE *fp, register PS_FontInfo *gfi)
{  
    BOOL cont = TRUE, save = (gfi != NULL);
    int error = ok;
    register char *keyword;
    
    while (cont)
    {
        keyword = token(fp);
        
        if (keyword == NULL)
          /* Have reached an early and unexpected EOF. */
          /* Set flag and stop parsing */
        {
            error = earlyEOF;
            break;   /* get out of loop */
        }
        if (!save)	
          /* get tokens until the end of the Global Font info section */
          /* without saving any of the data */
            switch (recognize(keyword))  
            {				
                case STARTCHARMETRICS:
                    cont = FALSE;
                    break;
                case ENDFONTMETRICS:	
                    cont = FALSE;
                    error = normalEOF;
                    break;
                default:
                    break;
            } /* switch */
        else {
          /* otherwise parse entire global font info section, */
          /* saving the data */
            switch(recognize(keyword))
            {
                case STARTFONTMETRICS:
                    keyword = token(fp);
                    break;
                case COMMENT:
                    keyword = linetoken(fp);
                    break;
                case FONTNAME:
                    keyword = token(fp);
                    gfi->name = (char *) malloc(strlen(keyword) + 1);
                    strcpy(gfi->name, keyword);
                    break;
                case ENCODINGSCHEME:
                    keyword = token(fp);
                    break; 
                case FULLNAME:
                    keyword = linetoken(fp);
                    break; 
                case FAMILYNAME:           
                   keyword = linetoken(fp);
                    break; 
                case WEIGHT:
                    keyword = token(fp);
                    break;
                case ITALICANGLE:
                    keyword = token(fp);
                    break;
                case ISFIXEDPITCH:
                    keyword = token(fp);
                    break; 
	            case UNDERLINEPOSITION:
                    keyword = token(fp);
	            gfi->upos = atoi(keyword);
                    break; 
                case UNDERLINETHICKNESS:
                    keyword = token(fp);
                    gfi->uthick = atoi(keyword);
                    break;
                case VERSION:
                    keyword = token(fp);
                    break; 
                case NOTICE:
                    keyword = linetoken(fp);
                    break; 
                case FONTBBOX:
                    keyword = token(fp);
                    gfi->fontBBox.llx = atoi(keyword);
                    keyword = token(fp);
                    gfi->fontBBox.lly = atoi(keyword);
                    keyword = token(fp);
                    gfi->fontBBox.urx = atoi(keyword);
                    keyword = token(fp);
                    gfi->fontBBox.ury = atoi(keyword);
                    break;
                case CAPHEIGHT:
                    keyword = token(fp);
                    break;
                case XHEIGHT:
                    keyword = token(fp);
                    break;
                case DESCENDER:
                    keyword = token(fp);
                    break;
                case ASCENDER:
                    keyword = token(fp);
                    break;
                case STARTCHARMETRICS:
                    cont = FALSE;
                    break;
                case ENDFONTMETRICS:
                    cont = FALSE;
                    error = normalEOF;
                    break;
                case NOPE:
                break;
            default: 
                error = parseError;
                break;
        } /* switch */
        }
    } /* while */
    
    return(error);
    
} /* parseGlobals */    

/************************* parseCharMetrics ************************/

static int parseCharMetrics(FILE *fp, register PS_FontInfo *fi)
{  
    BOOL cont = TRUE;
    int error = ok, count = 0, code = 0;
    register char *keyword;
  
    while (cont)
    {
        keyword = token(fp);
        if (keyword == NULL)
        {
            error = earlyEOF;
            break; /* get out of loop */
        }

        switch(recognize(keyword))
        {
            case COMMENT:
                keyword = linetoken(fp);
                break; 
            case CODE:
                    code = atoi(token(fp));
                    count++;
                break;
            case XYWIDTH:
                if (code <0 || code >= 256) {
                    token(fp); token(fp); break;
                }
                fi->chars[code].wx = atoi(token(fp));
                fi->chars[code].wy = atoi(token(fp));
                break;                 
            case XWIDTH: 
                if (code <0 || code >= 256) {
                    token(fp); break;
                }
                fi->chars[code].wx = atoi(token(fp));
                break;
            case CHARNAME: 
                keyword = token(fp);
                break;            
            case CHARBBOX: 
                if (code <0 || code >= 256) {
                    token(fp); token(fp); token(fp); token(fp); break;
                    }
                fi->chars[code].charBBox.llx = atoi(token(fp));
                fi->chars[code].charBBox.lly = atoi(token(fp));
                fi->chars[code].charBBox.urx = atoi(token(fp));
                fi->chars[code].charBBox.ury = atoi(token(fp));
                    break;
            case LIGATURE:
                        keyword = token(fp);
                        keyword = token(fp);
                    break;
            case ENDCHARMETRICS:
                cont = FALSE;;
                    break;
                case ENDFONTMETRICS:
                    cont = FALSE;
                    error = normalEOF;
                    break;
                case NOPE:
                    break;
                default:
                    error = parseError;
                    break;
            } /* switch */
    } /* while */
    
    return(error);
    
} /* parseCharMetrics */    



/*************************** 'PUBLIC' FUNCTION ********************/ 


/*************************** XP_parseAFMFile *****************************/

/*  this is the only 'public' procedure available. It is called 
 *  from an application wishing to get information from an AFM file.
 *  The caller of this function is responsible for locating and opening
 *  an AFM file and handling all errors associated with that task.
 *
 *  parseFile returns an error code as defined in parseAFM.h. 
 *
 *  The position of the read/write pointer associated with the file 
 *  pointer upon return of this function is undefined.
 */

int XP_parseAFMFile (fp, fi)
  FILE *fp;
  PS_FontInfo **fi;
{
    
    int code = ok; 	/* return code from each of the parsing routines */
    int error = ok;	/* used as the return code from this function */
    int i;
    
    register char *keyword; /* used to store a token */	 
    
   			      
    /* storage data for the global variable ident */			      
    ident = (char *) calloc(MAX_NAME, sizeof(char)); 
    if (ident == NULL) {error = storageProblem; return(error);}      
  
    if ((*fi) == NULL) (*fi) = (PS_FontInfo *) calloc(1, sizeof(PS_FontInfo));
    if ((*fi) == NULL) {error = storageProblem; return(error);}      
  
    for (i=0; i<256; i++) {	/* initialize the default font width */
    	(*fi)->chars[i].wx = 250;
    }
    
    /* The AFM File begins with Global Font Information. This section */
    /* will be parsed whether or not information should be saved. */     
    code = parseGlobals(fp, (*fi)); 

    /* be liberal to strict AFM grammar. */
    if ((code != earlyEOF) && (code < 0)) error = code;
    
    /* The Global Font Information is followed by the Character Metrics */
    /* section. */
  
    if ((code != normalEOF) && (code != earlyEOF))
    {
        token(fp);
        code = parseCharMetrics(fp, *fi);             
    } /* if */

    if ((code != earlyEOF) && (code < 0)) error = code;
    
    if (ident != NULL) { free(ident); ident = NULL; }
        
    return(error);
  
} /* XP_parseAFMFile */
