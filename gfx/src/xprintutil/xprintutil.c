/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the X11 print system utilities library.
 *
 * The Initial Developer of the Original Code is
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>. All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xprintutil.h"
 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <locale.h>

#ifdef XPU_USE_NSPR
#include "plstr.h"
#undef strtok_r
#define strtok_r(s1, s2, x) PL_strtok_r((s1), (s2), (x))
#endif /* XPU_USE_NSPR */

/* List of tokens which can be used to separate entries in the 
 * $XPSERVERLIST env var */
static const char XPServerListSeparators[] = " \t\v\n\r\f";

/* conformace only; X11 API does (currrently) not make use of |const|. 
 * If Xlib API gets fixed these macros can be turned into empty 
 * placeholders... (|#define MAKE_STRING_WRITABLE(x)|) :-)
 */
#define MAKE_STRING_WRITABLE(str) (((str)?((str) = strdup(str)):0))
#define FREE_WRITABLE_STRING(str) free((void *)(str))
#define STRING_AS_WRITABLE(str) ((char *)(str))

/* Local prototypes */
static const char *XpuGetDefaultXpPrintername(void);
static const char *XpuGetXpServerList( void );
static const char *XpuEnumerateXpAttributeValue( const char *value, void **vcptr );
static const char *XpuGetCurrentAttributeGroup( void **vcptr );
static void XpuDisposeEnumerateXpAttributeValue( void **vc );
static Bool XpuEnumerateMediumSourceSizes( Display *pdpy, XPContext pcontext,
                                           const char **tray_name,
                                           const char **medium_name, int *mbool, 
                                           float *ma1, float *ma2, float *ma3, float *ma4,
                                           void **vcptr );
static void XpuDisposeEnumerateMediumSourceSizes( void **vc );

/*
** XprintUtil functions start with Xpu
**
*/

int XpuCheckExtension( Display *pdpy )
{
  char *display = XDisplayString(pdpy);
  short major = 0,
        minor = 0;

  if( XpQueryVersion(pdpy, &major, &minor) != 0 )
  {
    XPU_DEBUG_ONLY(printf("XpuCheckExtension: XpQueryVersion '%s' %d %d\n", XPU_NULLXSTR(display), (int)major, (int)minor));
    return(1);
  }
  else
  {
    XPU_DEBUG_ONLY(printf("XpuCheckExtension: XpQueryVersion '%s' returned 0(=Xprint not supported)\n", XPU_NULLXSTR(display)));
  }
  
  return(0);
}

/* Get the default printer name from the XPRINTER env var.
 * If XPRINTER env var is not present looks for PDPRINTER, LPDEST, and
 * PRINTER (in that order)
 * See CDE's DtPrintSetupBox(3) manual page, too...
 */
static
const char *XpuGetDefaultXpPrintername(void)
{
  const char *s;
  /* BUG/TODO: XpPrinter resource needs to be sourced, too... */
  s = getenv("XPRINTER");
  if( !s )
  {
    s = getenv("PDPRINTER");
    if( !s )
    {
      s = getenv("LPDEST");
      if( !s )
      {
        s = getenv("PRINTER");
      }
    }
  }  
  return s;
}

static
const char *XpuGetXpServerList( void )
{
  const char *s;
  /* BUG/TODO: XpServerList resource needs to be sourced first, then append 
   * contents of XPSERVERLIST, then remove duplicates...
   */
  s = getenv("XPSERVERLIST"); 
  if( s == NULL )
    s = "";
    
  return(s);
}


Bool XpuXprintServersAvailable( void )
{
  const char *s;
  int         c = 0;
  /* BUGs/ToDo:
   * - XpServerList resource needs to be sourced, too...
   *   (see comment for |XpuGetXpServerList|, too)
   * - There should be some validation whether the server entries
   *   are
   *     a) valid (easy :)
   *   and
   *     b) available (hard to implement when XOpenDisplay() should be avoided)
   */
  s = getenv("XPSERVERLIST");
  /* Check if serverlist is non-empty */
  if (s)
  {
    while( *s++ )
    {
      if( !isspace(*s) )
        c++;
    }
  }
  /* a valid server name must at least contain the ':'-seperator
   * and a number (e.g. ":1") */
  return( c >= 2 );
}


static 
int XpuGetPrinter2( char *printer, char *display, Display **pdpyptr, XPContext *pcontextptr )
{
  Display   *pdpy;
  XPContext  pcontext;
  
  XPU_DEBUG_ONLY(printf("XpuGetPrinter2: probing display '%s' for '%s'\n", XPU_NULLXSTR(display), XPU_NULLXSTR(printer)));
  
  if( (pdpy = XOpenDisplay(display)) != NULL )
  {
    if( XpuCheckExtension(pdpy) )
    {
      XPPrinterList list;
      int           list_count;
      
      /* get list of available printers... */
      list = XpGetPrinterList(pdpy, printer, &list_count);        
      if( list != NULL ) XpFreePrinterList(list);
      
      /* ...and check if printer exists... */
      if( (list != NULL) && (list_count > 0) )
      {
        if( (pcontext = XpCreateContext(pdpy, printer)) != None )
        {
          *pdpyptr     = pdpy;
          *pcontextptr = pcontext;
          return(1);
        }
        
        XPU_DEBUG_ONLY(printf("XpuGetPrinter2: could not create print context for '%s'\n", XPU_NULLXSTR(printer)));
      }
    }
    else
    {
      XPU_DEBUG_ONLY(printf("display '%s' does not support the Xprint extension\n", XPU_NULLXSTR(display)));
    }  

    XCloseDisplay(pdpy);
    return(0);
  }
  else
  {
    XPU_DEBUG_ONLY(printf("could not open display '%s'\n", XPU_NULLXSTR(display)));
    return(0);
  }
}


/* acceps "printer" or "printer@display" */
int XpuGetPrinter( const char *arg_printername, Display **pdpyptr, XPContext *pcontextptr )
{
  Display       *pdpy;
  XPContext      pcontext;
  char          *printername;
  char          *s;
  char          *tok_lasts;
  
  *pdpyptr     = NULL;
  *pcontextptr = None;
  
  XPU_DEBUG_ONLY(printf("XpuGetPrinter: looking for '%s'\n", XPU_NULLXSTR(arg_printername)));
  
  /* strtok_r will modify string - duplicate it first... */
  printername = strdup(arg_printername);
  if( printername == NULL )
    return(0);
  
  if( (s = strtok_r(printername, "@", &tok_lasts)) != NULL )
  {
    char *name = s;
    char *display = strtok_r(NULL, "@", &tok_lasts);
    
    /* if we have a display - open it and grab printer */
    if( display != NULL )
    {
      if( XpuGetPrinter2(name, display, pdpyptr, pcontextptr) )
      {
        free(printername);
        return(1);
      }
    }
    /* if we did not get a display, travel througth all displays */
    else
    {
      char *sl = strdup(XpuGetXpServerList());
      
      if( sl != NULL )
      {
        for( display = strtok_r(sl, XPServerListSeparators, &tok_lasts) ; 
             display != NULL ; 
             display = strtok_r(NULL, XPServerListSeparators, &tok_lasts) )
        {       
          if( XpuGetPrinter2(name, display, pdpyptr, pcontextptr) )
          {
            free(sl);
            free(printername);
            return(1);
          } 
        }
        
        free(sl);
      }
    }
  }
  
  free(printername);
  XPU_DEBUG_ONLY(printf("XpuGetPrinter: failure\n"));
  
  return(0);
}


void XpuClosePrinterDisplay(Display *pdpy, XPContext pcontext)
{
  if( pdpy )
  {
    if( pcontext != None )
      XpDestroyContext(pdpy, pcontext);
      
    XCloseDisplay(pdpy);
  }
}

void XpuSetOneAttribute( Display *pdpy, XPContext pcontext, 
                         XPAttributes type, const char *attribute_name, const char *value, XPAttrReplacement replacement_rule )
{
  /* Alloc buffer for sprintf() stuff below */
  char *buffer = (char *)malloc(strlen(attribute_name)+strlen(value)+4);
  
  if( buffer != NULL )
  {
    sprintf(buffer, "%s: %s", attribute_name, value);      
    XpSetAttributes(pdpy, pcontext, type, buffer, replacement_rule);
    free(buffer);
  }  
}

void XpuSetOneLongAttribute( Display *pdpy, XPContext pcontext, 
                             XPAttributes type, const char *attribute_name, long value, XPAttrReplacement replacement_rule )
{
  /* Alloc buffer for sprintf() stuff below */
  char *buffer = (char *)malloc(strlen(attribute_name)+32+4);
  
  if( buffer != NULL )
  {
    sprintf(buffer, "%s: %ld", attribute_name, value);      
    XpSetAttributes(pdpy, pcontext, type, buffer, replacement_rule);
    free(buffer);
  }  
}

/* Check if attribute value is supported or not
 * Use this function _only_ if XpuGetSupported{Job,Doc,Page}Attributes()
 * does not help you...
 */
int XpuCheckSupported( Display *pdpy, XPContext pcontext, XPAttributes type, const char *attribute_name, const char *query )
{
  char *value;
  void *tok_lasts;
  
  MAKE_STRING_WRITABLE(attribute_name);
  if( attribute_name == NULL )
    return(0);
    
  value = XpGetOneAttribute(pdpy, pcontext, type, STRING_AS_WRITABLE(attribute_name));   
  
  XPU_DEBUG_ONLY(printf("XpuCheckSupported: XpGetOneAttribute(%s) returned '%s'\n", XPU_NULLXSTR(attribute_name), XPU_NULLXSTR(value)));

  FREE_WRITABLE_STRING(attribute_name);
  
  if( value != NULL )
  {
    const char *s;
    
    for( s = XpuEnumerateXpAttributeValue(value, &tok_lasts) ; s != NULL ; s = XpuEnumerateXpAttributeValue(NULL, &tok_lasts) )
    {
      XPU_DEBUG_ONLY(printf("XpuCheckSupported: probing '%s'=='%s'\n", XPU_NULLXSTR(s), XPU_NULLXSTR(query)));
      if( !strcmp(s, query) )
      {
        XFree(value);
        XpuDisposeEnumerateXpAttributeValue(&tok_lasts);
        return(1);
      }  
    }
    
    XpuDisposeEnumerateXpAttributeValue(&tok_lasts);
    XFree(value);
  }  
  
  return(0);
}


int XpuSetJobTitle( Display *pdpy, XPContext pcontext, const char *title )
{
  if( XpuGetSupportedJobAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_JOB_NAME )
  {
    char *encoded_title;
    
    encoded_title = XpuResourceEncode(title);
    if (!encoded_title)
      return(0);
    XpuSetOneAttribute(pdpy, pcontext, XPJobAttr, "*job-name", encoded_title, XPAttrMerge);
    XpuResourceFreeString(encoded_title);
    return(1);
  }
  else
  {
    XPU_DEBUG_ONLY(printf("XpuSetJobTitle: XPUATTRIBUTESUPPORTED_JOB_NAME not supported ('%s')\n", XPU_NULLXSTR(title)));
    return(0); 
  }  
}
    
int XpuGetOneLongAttribute( Display *pdpy, XPContext pcontext, XPAttributes type, const char *attribute_name, long *result )
{
  char *s;
  
  MAKE_STRING_WRITABLE(attribute_name);
  if( attribute_name == NULL )
    return(0);
  s = XpGetOneAttribute(pdpy, pcontext, type, STRING_AS_WRITABLE(attribute_name));
  
  if(s && *s) 
  {
    long tmp;
    
    XPU_DEBUG_ONLY(printf("XpuGetOneLongAttribute: '%s' got '%s'\n", XPU_NULLXSTR(attribute_name), XPU_NULLXSTR(s)));
    
    tmp = strtol(s, (char **)NULL, 10);
    
    if( !(((tmp == 0L) || (tmp == LONG_MIN) || (tmp == LONG_MAX)) && 
          ((errno == ERANGE) || (errno == EINVAL))) )
    {
      *result = tmp;
      XFree(s);
      XPU_DEBUG_ONLY(printf("XpuGetOneLongAttribute: result %ld\n", *result));
      FREE_WRITABLE_STRING(attribute_name);
      return(1);
    }            
  }
  
  if( s != NULL ) 
    XFree(s);
  
  FREE_WRITABLE_STRING(attribute_name);
  
  return(0);
}


#ifdef DEBUG
/* debug only */
void dumpXpAttributes( Display *pdpy, XPContext pcontext )
{
  char *s;
  printf("------------------------------------------------\n");
  printf("--> Job\n%s\n",         s=XpuGetJobAttributes(pdpy, pcontext));     XFree(s);
  printf("--> Doc\n%s\n",         s=XpuGetDocAttributes(pdpy, pcontext));     XFree(s);
  printf("--> Page\n%s\n",        s=XpuGetPageAttributes(pdpy, pcontext));    XFree(s);
  printf("--> Printer\n%s\n",     s=XpuGetPrinterAttributes(pdpy, pcontext)); XFree(s);
  printf("--> Server\n%s\n",      s=XpuGetServerAttributes(pdpy, pcontext));  XFree(s);
  printf("image resolution %d\n", (int)XpGetImageResolution(pdpy, pcontext));
  printf("------------------------------------------------\n");
}
#endif /* DEBUG */    


typedef struct XpuIsNotifyEventContext_
{
  int event_base;
  int detail;
} XpuIsNotifyEventContext;

static
Bool IsXpNotifyEvent( Display *pdpy, XEvent *ev, XPointer arg )
{
  Bool match;
  XpuIsNotifyEventContext *context = (XpuIsNotifyEventContext *)arg;
  XPPrintEvent *pev = (XPPrintEvent *)ev;
  
  match = pev->type == (context->event_base+XPPrintNotify) && 
          pev->detail == context->detail;

  XPU_DEBUG_ONLY(printf("XpuWaitForPrintNotify: %d=IsXpNotifyEvent(%d,%d)\n",
                 (int)match,
                 (int)pev->type,
                 (int)pev->detail));
  return match;
}

void XpuWaitForPrintNotify( Display *pdpy, int xp_event_base, int detail )
{
  XEvent                  dummy;
  XpuIsNotifyEventContext matchcontext;

  matchcontext.event_base = xp_event_base;
  matchcontext.detail     = detail;
  XIfEvent(pdpy, &dummy, IsXpNotifyEvent, (XPointer)&matchcontext);
}      

static
const char *skip_matching_brackets(const char *start)
{
  const char *s     = start;
  int         level = 0;
  
  if( !start )
      return(NULL);
  
  do
  {
    switch(*s++)
    {
      case '\0': return(NULL);
      case '{': level++; break;
      case '}': level--; break;
    }
  } while(level > 0);

  return(s);
}


static
const char *search_next_space(const char *start)
{
  const char *s     = start;
  int         level = 0;
  
  if( !start )
    return(NULL);
  
  for(;;)
  {   
    if( isspace(*s) )
      return(s);

    if( *s=='\0' )
      return(NULL);      

    s++;
  }
}

/* PRIVATE context data for XpuEnumerateXpAttributeValue() */
typedef struct _XpuAttributeValueEnumeration
{
  char   *value;
  size_t  original_value_len; /* original length of value */
  char   *group;
  char   *start;
  char   *s;
} XpuAttributeValueEnumeration;


/* Hacked parser for Xp values and enumerations */
static
const char *XpuEnumerateXpAttributeValue( const char *value, void **vcptr )
{
  XpuAttributeValueEnumeration **cptr = (XpuAttributeValueEnumeration **)vcptr;
  XpuAttributeValueEnumeration  *context;
  const char                    *tmp;
  
  if( !cptr )
    return(NULL);
    
  if( value )
  {
    XpuAttributeValueEnumeration *e;
    const char *s = value;
    Bool        isGroup = FALSE;
  
    e = (XpuAttributeValueEnumeration *)malloc(sizeof(XpuAttributeValueEnumeration));
    if( !e )
      return NULL;
  
    /* Skip leading '{'. */
    while(*s=='{' && isGroup==FALSE)
    {
      s++;
      isGroup = TRUE;
    }  
    /* Skip leading blanks. */
    while(isspace(*s))
      s++;
    
    e->group = NULL;
    
    /* Read group name. */
    if( isGroup )
    { 
      tmp = s;  
      while(!isspace(*s))
        s++;
      if(strncmp(tmp, "''", s-tmp) != 0)
      {
        e->group = strdup(tmp);
        e->group[s-tmp] = '\0';
      }
    }
  
    e->original_value_len = strlen(s);
    e->value = (char *)malloc(e->original_value_len+4); /* We may look up to three bytes beyond the string */
    strcpy(e->value, s);
    memset(e->value+e->original_value_len+1, 0, 3); /* quad termination */
    e->start = e->s = e->value;
    
    *cptr = e;
  }
  
  context = *cptr;
  
  if( !context || !context->s )
    return(NULL);
   
  /* Skip leading blanks, '\'' or '}' */
  while(isspace(*(context->s)) || *(context->s)=='\'' /*|| *(context->s)=='}'*/ )
    context->s++;

  if( *(context->s) == '\0' )
    return(NULL);

  context->start = context->s;
  if( *(context->start) == '{' )
    context->s = (char *)skip_matching_brackets(context->start);
  else
    context->s = (char *)search_next_space(context->start);
    
  /* end of string reached ? */
  if( context->s )
  {   
    *(context->s) = '\0';
    context->s++;
  }
  
  /* Check if we reached a new attribute group */
  tmp = context->start;
  while(isspace(*tmp))
    tmp++;   
  if( *tmp=='}' )
  {
    void *prev_cptr = *vcptr;
    
    tmp+=2; /* We have 3*'\0' at the end of the string - this is legal! */
    if( *tmp!='\0' )
    {
      const char *ret;
   
      /* Start the parser again */
      *vcptr = NULL;
      ret = XpuEnumerateXpAttributeValue(tmp, vcptr);
    
      /* Free old context */
      XpuDisposeEnumerateXpAttributeValue(&prev_cptr);
    
      return(ret);
    }
    else
    {
      return(NULL);
    }
  }
  
  return(context->start);   
}

/* Get enumeration group for last string returned by |XpuEnumerateXpAttributeValue|... */
static
const char *XpuGetCurrentAttributeGroup( void **vcptr )
{
  XpuAttributeValueEnumeration **cptr = (XpuAttributeValueEnumeration **)vcptr;
  if( !cptr )
    return(NULL);
  if( !*cptr )
    return(NULL);
    
  return((*cptr)->group);
}


static
void XpuDisposeEnumerateXpAttributeValue( void **vc )
{ 
  if( vc )
  {
    XpuAttributeValueEnumeration *context = *((XpuAttributeValueEnumeration **)vc);
    free(context->value);
    if(context->group)
      free(context->group);
    free(context);   
  }
}

/* parse a paper size string 
 * (example: '{na-letter False {6.3500 209.5500 6.3500 273.0500}}') */
static
Bool XpuParseMediumSourceSize( const char *value, 
                               const char **medium_name, int *mbool, 
                               float *ma1, float *ma2, float *ma3, float *ma4 )
{
  const char *s;
  char       *d, 
             *name;
  char       *boolbuf;
  size_t      value_len;
  int         num_input_items;
  const char *cur_locale;
  
  if( value && value[0]!='{' && value[0]!='\0' )
    return(False);
    
  value_len = strlen(value);
  
  /* alloc buffer for |medium_name| and |boolbuf| in one step
   * (both must be large enougth to hold at least |strlen(value)+1| bytes) */  
  name = (char *)malloc(value_len*2 + 4);
  boolbuf = name + value_len+2; /* |boolbuf| starts directly after |name| */
  
  /* remove '{' && '}' */
  s = value;
  d = name;
  do
  {
    *d = tolower(*s);
    
    if( *s!='{' && *s!='}' )
      d++;
    
    s++;  
  }
  while(*s);
  *d = '\0';
    
  /* separate medium name from string */
  d = (char *)search_next_space(name);
  if( !d )
  {
    free(name);
    return(False);
  }  
  *d = '\0';
  *medium_name = name;
  
  /* ... continue to parse the remaining string... */
  d++;
  

  /* Force C/POSIX radix for scanf()-parsing (see bug 131831 ("Printing
   * does not work in de_AT@euro locale")), do the parsing and restore
   * the original locale.
   * XXX: This may affect all threads and not only the calling one...
   */
  {
#define CUR_LOCALE_SIZE 256
    char cur_locale[CUR_LOCALE_SIZE+1];
    strncpy(cur_locale, setlocale(LC_NUMERIC, NULL), CUR_LOCALE_SIZE);
    cur_locale[CUR_LOCALE_SIZE]='\0';
    setlocale(LC_NUMERIC, "C"); 
    num_input_items = sscanf(d, "%s %f %f %f %f", boolbuf, ma1, ma2, ma3, ma4);
    setlocale(LC_NUMERIC, cur_locale);
#undef CUR_LOCALE_SIZE
  }

  if( num_input_items != 5 )
  {
    free(name);
    return(False);
  }

  if( !strcmp(boolbuf, "true") )
    *mbool = True;
  else if( !strcmp(boolbuf, "false") )
    *mbool = False;
  else
  {
    free(name);
    return(False);    
  }
  return(True);
}


/* parse a paper size string 
 * (example: '{na-letter False {6.3500 209.5500 6.3500 273.0500}}') */
static
Bool XpuEnumerateMediumSourceSizes( Display *pdpy, XPContext pcontext,
                                    const char **tray_name,
                                    const char **medium_name, int *mbool, 
                                    float *ma1, float *ma2, float *ma3, float *ma4,
                                    void **vcptr )
{
  const char *medium_spec;
  const char *value = NULL;
  
  if( pdpy && pcontext )
  {
    value = XpGetOneAttribute(pdpy, pcontext, XPPrinterAttr, "medium-source-sizes-supported");
    if( !value )
      return(False);
  }

  while(1)
  {  
    medium_spec = XpuEnumerateXpAttributeValue(value, vcptr);
    
    if( value )
    {
      XFree((void *)value);
      value = NULL;
    }

    /* enumeration done? */
    if( !medium_spec )
      return(False);

    if (XpuParseMediumSourceSize(medium_spec, 
                                 medium_name, mbool, 
                                 ma1, ma2, ma3, ma4))
    {
      *tray_name = XpuGetCurrentAttributeGroup(vcptr);
      return(True);
    }
    else
    {
      /* Should never ever happen! */
      fprintf(stderr, "XpuEnumerateMediumSourceSize: error parsing '%s'\n", medium_spec);
    }
  }
  /* not reached */   
}

static
void XpuDisposeEnumerateMediumSourceSizes( void **vc )
{
  XpuDisposeEnumerateXpAttributeValue(vc);
}  


/* future: Migrate this functionality into |XpGetPrinterList| - just do
 * not pass a |Display *| to |XpGetPrinterList|
 */
XPPrinterList XpuGetPrinterList( const char *printer, int *res_list_count )
{
  XPPrinterRec *rec = NULL;
  int           rec_count = 1; /* Allocate one more |XPPrinterRec| structure
                                * as terminator */
  char         *sl;
  const char   *default_printer_name = XpuGetDefaultXpPrintername();
  int           default_printer_rec_index = -1;

  if( !res_list_count )
    return(NULL); 
  
  sl = strdup(XpuGetXpServerList());
  MAKE_STRING_WRITABLE(printer);
    
  if( sl != NULL )
  {
    char *display;
    char *tok_lasts;
    
    for( display = strtok_r(sl, XPServerListSeparators, &tok_lasts) ; 
         display != NULL ; 
         display = strtok_r(NULL, XPServerListSeparators, &tok_lasts) )
    {
      Display *pdpy;
      
      if( (pdpy = XOpenDisplay(display)) != NULL )
      {
        XPPrinterList list;
        int           list_count;
        size_t        display_len = strlen(display);

        /* get list of available printers... */
        list = XpGetPrinterList(pdpy, STRING_AS_WRITABLE(printer), &list_count);        
      
        if( list && list_count )
        {
          int i;
          
          for( i = 0 ; i < list_count ; i++ )
          {
            char *s;
            
            /* Workaround for http://bugzilla.mozilla.org/show_bug.cgi?id=193499 
             * ("Xprint print/print preview crashes Mozilla") where the Solaris
             * Xprt may create invalid entries (e.g. |XpGetPrinterList| will
             * return |list[i].name==NULL| due to empty lines in the printer list.
             */
            if( !list[i].name )
              continue;
            
            rec_count++;
            rec = (XPPrinterRec *)realloc(rec, sizeof(XPPrinterRec)*rec_count);
            if( !rec ) /* failure */
              break;
              
            s = (char *)malloc(strlen(list[i].name)+display_len+4);
            sprintf(s, "%s@%s", list[i].name, display);
            rec[rec_count-2].name = s;
            rec[rec_count-2].desc = (list[i].desc)?(strdup(list[i].desc)):(NULL);
            
            /* Test for default printer (if the user set one).*/
            if( default_printer_name )
            {
              /* Default_printer_name may either contain the FQPN(=full
               * qualified printer name ("foo@myhost:5") or just the name
               * ("foo")) */
              if( (!strcmp(list[i].name, default_printer_name)) ||
                  (!strcmp(s,            default_printer_name)) )
              {
                /* Remember index of default printer that we can swap it to 
                 * the head of the array below... */
                default_printer_rec_index = rec_count-2;
              }
            }  
          }
          
          XpFreePrinterList(list);
        }
                 
        XCloseDisplay(pdpy);
      }
    }
    
    free(sl);
  }
  
  if( rec )
  {
    /* users: DO NOT COUNT ON THIS DETAIL 
     * (this is only to make current impl. of XpuFreePrinterList() easier)
     * I may remove this implementation detail in a later revision of
     * the library!
     */
    rec[rec_count-1].name = NULL;
    rec[rec_count-1].desc = NULL;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }
  
  /* The default printer is always the first one in the printer list... */
  if( (default_printer_rec_index != -1) && rec )
  {
    XPPrinterRec tmp;
    tmp = rec[0];
    rec[0] = rec[default_printer_rec_index];
    rec[default_printer_rec_index] = tmp;
  }
    
  *res_list_count = rec_count;
  FREE_WRITABLE_STRING(printer);
  return(rec);
}      


void XpuFreePrinterList( XPPrinterList list )
{
  if( list )
  {
    XPPrinterRec *curr = list;
  
    /* See the warning abouve about using this implementation detail for
     * checking for the list's end... */
    while( curr->name != NULL )
    {
      free(curr->name);
      if(curr->desc)
        free(curr->desc);
      curr++;
    }
  
    free(list);
  }
}

/* Set number of copies to print from this document */
int XpuSetDocumentCopies( Display *pdpy, XPContext pcontext, long num_copies )
{
  if( XpuGetSupportedDocAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_COPY_COUNT)
  {
    XpuSetOneLongAttribute(pdpy, pcontext, XPDocAttr, "*copy-count", num_copies, XPAttrMerge);
    return(1);
  }
  else
  {
    XPU_DEBUG_ONLY(printf("XpuSetContentOrientation: XPUATTRIBUTESUPPORTED_COPY_COUNT not supported\n"));
       
    /* Failure... */
    return(0);
  }  
}

XpuMediumSourceSizeList XpuGetMediumSourceSizeList( Display *pdpy, XPContext pcontext, int *numEntriesPtr )
{
  XpuMediumSourceSizeList list = NULL;
  int                     rec_count = 1; /* allocate one more |XpuMediumSourceSizeRec| structure
                                          * as terminator */
  Bool                    status;
  float                   ma1,
                          ma2,
                          ma3,
                          ma4;
  char                   *value;
  void                   *tok_lasts;
  const char             *tray_name,
                         *medium_name;
  int                     mbool;
  const char             *default_tray,
                         *default_medium;
  int                     default_medium_rec_index = -1;
  
  default_tray   = XpGetOneAttribute(pdpy, pcontext, XPDocAttr, "default-input-tray");
  if(!default_tray)
  {
    fprintf(stderr, "XpuGetMediumSourceSizeList: Internal error, no 'default-input-tray' found.\n");
    return(NULL);
  }
  default_medium = XpGetOneAttribute(pdpy, pcontext, XPDocAttr, "default-medium");
  if(!default_medium)
  {
    fprintf(stderr, "XpuGetMediumSourceSizeList: Internal error, no 'default-medium' found.\n");
    XFree((void *)default_tray);
    return(NULL);
  }
  
  for( status = XpuEnumerateMediumSourceSizes(pdpy, pcontext, &tray_name, &medium_name, &mbool,
                                              &ma1, &ma2, &ma3, &ma4, &tok_lasts) ;
       status != False ;
       status = XpuEnumerateMediumSourceSizes(NULL, None,     &tray_name, &medium_name, &mbool, 
                                              &ma1, &ma2, &ma3, &ma4, &tok_lasts) )
  {
    rec_count++;
    list = (XpuMediumSourceSizeRec *)realloc(list, sizeof(XpuMediumSourceSizeRec)*rec_count);
    if( !list )
      return(NULL);
    
    list[rec_count-2].tray_name   = (tray_name)?(strdup(tray_name)):(NULL);
    list[rec_count-2].medium_name = strdup(medium_name);
    list[rec_count-2].mbool       = mbool;
    list[rec_count-2].ma1         = ma1;
    list[rec_count-2].ma2         = ma2;
    list[rec_count-2].ma3         = ma3;
    list[rec_count-2].ma4         = ma4;
    
    /* Default medium ? */
    if( (!strcmp(medium_name, default_medium)) && 
        ((tray_name && (*default_tray))?(!strcmp(tray_name, default_tray)):(True)) )
    {
      default_medium_rec_index = rec_count-2;
    }
  }  

  XpuDisposeEnumerateMediumSourceSizes(&tok_lasts);

  if( list )
  {
    /* users: DO NOT COUNT ON THIS DETAIL 
     * (this is only to make current impl. of XpuFreeMediumSourceSizeList() easier)
     * I may remove this implementation detail in a later revision of
     * the library! */
    list[rec_count-1].tray_name  = NULL;
    list[rec_count-1].medium_name = NULL;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }

  /* Make the default medium always the first item in the list... */
  if( (default_medium_rec_index != -1) && list )
  {
    XpuMediumSourceSizeRec tmp;
    tmp = list[0];
    list[0] = list[default_medium_rec_index];
    list[default_medium_rec_index] = tmp;
  }

  *numEntriesPtr = rec_count; 
  return(list);
}

void XpuFreeMediumSourceSizeList( XpuMediumSourceSizeList list )
{
  if( list )
  {
    XpuMediumSourceSizeRec *curr = list;
  
    /* See the warning abouve about using this implementation detail for
     * checking for the list's end... */
    while( curr->medium_name != NULL )
    {
      if( curr->tray_name)
        free((void *)curr->tray_name);
      free((void *)curr->medium_name);
      curr++;
    }
  
    free(list);
  }
}

static
int XpuSetMediumSourceSize( Display *pdpy, XPContext pcontext, XPAttributes type, XpuMediumSourceSizeRec *medium_spec )
{
  /* Set the "default-medium" and "*default-input-tray" 
   * (if |XpuEnumerateMediumSourceSizes| returned one) XPDocAttr's
   * attribute and return */
  if (medium_spec->tray_name)
  {
    XpuSetOneAttribute(pdpy, pcontext, type, "*default-input-tray", medium_spec->tray_name, XPAttrMerge);
  }
  XpuSetOneAttribute(pdpy, pcontext, type, "*default-medium", medium_spec->medium_name, XPAttrMerge);
  
  return( 1 );
}

/* Set document medium size */
int XpuSetDocMediumSourceSize( Display *pdpy, XPContext pcontext, XpuMediumSourceSizeRec *medium_spec )
{
  XpuSupportedFlags doc_supported_flags;
  
  doc_supported_flags = XpuGetSupportedDocAttributes(pdpy, pcontext);

  if( (doc_supported_flags & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM) == 0 )
    return( 0 );
    
  if (medium_spec->tray_name)
  {
    if( (doc_supported_flags & XPUATTRIBUTESUPPORTED_DEFAULT_INPUT_TRAY) == 0 )
      return( 0 );  
  }

  return XpuSetMediumSourceSize(pdpy, pcontext, XPDocAttr, medium_spec);
}

/* Set page medium size */
int XpuSetPageMediumSourceSize( Display *pdpy, XPContext pcontext, XpuMediumSourceSizeRec *medium_spec )
{
  XpuSupportedFlags page_supported_flags;
  
  page_supported_flags = XpuGetSupportedPageAttributes(pdpy, pcontext);

  if( (page_supported_flags & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM) == 0 )
    return( 0 );
    
  if (medium_spec->tray_name)
  {
    if( (page_supported_flags & XPUATTRIBUTESUPPORTED_DEFAULT_INPUT_TRAY) == 0 )
      return( 0 );  
  }

  return XpuSetMediumSourceSize(pdpy, pcontext, XPPageAttr, medium_spec);
}

#ifndef ABS
#define ABS(x) ((x)<0?-(x):(x))
#endif /* ABS */
#define MORE_OR_LESS_EQUAL(a, b, tolerance) (ABS((a) - (b)) <= (tolerance))

XpuMediumSourceSizeRec *
XpuFindMediumSourceSizeBySize( XpuMediumSourceSizeList mlist, int mlist_count, 
                               float page_width_mm, float page_height_mm, float tolerance )
{
  int i;
  for( i = 0 ; i < mlist_count ; i++ )
  {
    XpuMediumSourceSizeRec *curr = &mlist[i];
    float total_width  = curr->ma1 + curr->ma2,
          total_height = curr->ma3 + curr->ma4;

    /* Match width/height*/
    if( ((page_width_mm !=-1.f)?(MORE_OR_LESS_EQUAL(total_width,  page_width_mm,  tolerance)):(True)) &&
        ((page_height_mm!=-1.f)?(MORE_OR_LESS_EQUAL(total_height, page_height_mm, tolerance)):(True)) )
    {
      return(curr);
    }
  }

  return(NULL);
}

XpuMediumSourceSizeRec *
XpuFindMediumSourceSizeByBounds( XpuMediumSourceSizeList mlist, int mlist_count, 
                                 float m1, float m2, float m3, float m4, float tolerance )
{
  int i;
  for( i = 0 ; i < mlist_count ; i++ )
  {
    XpuMediumSourceSizeRec *curr = &mlist[i];

    /* Match bounds */
    if( ((m1!=-1.f)?(MORE_OR_LESS_EQUAL(curr->ma1, m1, tolerance)):(True)) &&
        ((m2!=-1.f)?(MORE_OR_LESS_EQUAL(curr->ma2, m2, tolerance)):(True)) &&
        ((m3!=-1.f)?(MORE_OR_LESS_EQUAL(curr->ma3, m3, tolerance)):(True)) &&
        ((m4!=-1.f)?(MORE_OR_LESS_EQUAL(curr->ma4, m4, tolerance)):(True)) )
    {
      return(curr);
    }
  }

  return(NULL);
}

XpuMediumSourceSizeRec *
XpuFindMediumSourceSizeByName( XpuMediumSourceSizeList mlist, int mlist_count, 
                               const char *tray_name, const char *medium_name )
{
  int i;
  for( i = 0 ; i < mlist_count ; i++ )
  {
    XpuMediumSourceSizeRec *curr = &mlist[i];

    /* Match by tray name and/or medium name */
    if( ((tray_name && curr->tray_name)?(!strcasecmp(curr->tray_name, tray_name)):(tray_name==NULL)) &&
        ((medium_name)?(!strcasecmp(curr->medium_name, medium_name)):(True)) )
    {
      return(curr);
    }
  }

  return(NULL);
}

XpuResolutionList XpuGetResolutionList( Display *pdpy, XPContext pcontext, int *numEntriesPtr )
{
  XpuResolutionList list = NULL;
  int               rec_count = 1; /* Allocate one more |XpuResolutionRec| structure
                                    * as terminator */
  char             *value;
  char             *tok_lasts;
  const char       *s;
  long              default_resolution = -1;
  int               default_resolution_rec_index = -1;
  char              namebuf[64];

  /* Get default document resolution */
  if( XpuGetOneLongAttribute(pdpy, pcontext, XPDocAttr, "default-printer-resolution", &default_resolution) != 1 )
  {
    default_resolution = -1;
  }
  
  value = XpGetOneAttribute(pdpy, pcontext, XPPrinterAttr, "printer-resolutions-supported");
  if (!value)
  {
    fprintf(stderr, "XpuGetResolutionList: Internal error, no 'printer-resolutions-supported' XPPrinterAttr found.\n");
    return(NULL);
  }
  
  for( s = strtok_r(value, " ", &tok_lasts) ;
       s != NULL ;
       s = strtok_r(NULL, " ", &tok_lasts) )
  {
    long tmp;
    
    tmp = strtol(s, (char **)NULL, 10);
    
    if( ((tmp == 0L) || (tmp == LONG_MIN) || (tmp == LONG_MAX)) && 
        ((errno == ERANGE) || (errno == EINVAL)) )
    {
      fprintf(stderr, "XpuGetResolutionList: Internal parser errror for '%s'.\n", s);
      continue;
    }    
  
    rec_count++;
    list = (XpuResolutionRec *)realloc(list, sizeof(XpuResolutionRec)*rec_count);
    if( !list )
      return(NULL);
    
    sprintf(namebuf, "%lddpi", tmp);
    list[rec_count-2].name   = strdup(namebuf);
    list[rec_count-2].x_dpi  = tmp;
    list[rec_count-2].y_dpi  = tmp;

    if( default_resolution != -1 )
    {
      /* Is this the default resolution ? */
      if( (list[rec_count-2].x_dpi == default_resolution) &&
          (list[rec_count-2].y_dpi == default_resolution) )
      {
        default_resolution_rec_index = rec_count-2;
      }
    }  
  }  

  XFree(value);

  if( list )
  {
    /* users: DO NOT COUNT ON THIS DETAIL 
     * (this is only to make current impl. of XpuGetResolutionList() easier)
     * We may remove this implementation detail in a later revision of
     * the library! */
    list[rec_count-1].name   = NULL;
    list[rec_count-1].x_dpi  = -1;
    list[rec_count-1].y_dpi  = -1;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }

  /* Make the default resolution always the first item in the list... */
  if( (default_resolution_rec_index != -1) && list )
  {
    XpuResolutionRec tmp;
    tmp = list[0];
    list[0] = list[default_resolution_rec_index];
    list[default_resolution_rec_index] = tmp;
  }

  *numEntriesPtr = rec_count; 
  return(list);
}

void XpuFreeResolutionList( XpuResolutionList list )
{
  if( list )
  { 
    XpuResolutionRec *curr = list;
  
    /* See the warning abouve about using this implementation detail for
     * checking for the list's end... */
    while( curr->name != NULL )
    {
      free((void *)curr->name);
      curr++;
    }  

    free(list);
  }
}

/* Find resolution in resolution list.
 */
XpuResolutionRec *XpuFindResolutionByName( XpuResolutionList list, int list_count, const char *name)
{
  int i;
  
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuResolutionRec *curr = &list[i];
    if (!strcasecmp(curr->name, name))
      return curr;
  }

  return NULL;
}

/* Get default page (if defined) or document resolution
 * this function may fail in the following conditions:
 * - No default resolution set yet
 * - X DPI != Y DPI (not yet implemented in Xprt)
 */
Bool XpuGetResolution( Display *pdpy, XPContext pcontext, long *x_dpi_ptr, long *y_dpi_ptr )
{
  long dpi;

  /* Try to get the current page's resolution (pages may differ in resolution if the DDX supports this) */
  if( XpuGetOneLongAttribute(pdpy, pcontext, XPPageAttr, "default-printer-resolution", &dpi) == 1 )
  {
    *x_dpi_ptr = dpi;
    *y_dpi_ptr = dpi;
    return True;
  }

  /* Get document resolution */
  if( XpuGetOneLongAttribute(pdpy, pcontext, XPDocAttr, "default-printer-resolution", &dpi) == 1 )
  {
    *x_dpi_ptr = dpi;
    *y_dpi_ptr = dpi;
    return True;
  }

  return False;
}

static
int XpuSetResolution( Display *pdpy, XPContext pcontext, XPAttributes type, XpuResolutionRec *rec )
{
  if( rec->x_dpi != rec->y_dpi )
  {
    fprintf(stderr, "XpuSetResolution: internal error: x_dpi != y_dpi not supported yet.\n");
    return 0;
  }

  XpuSetOneLongAttribute(pdpy, pcontext, type, "*default-printer-resolution", rec->x_dpi, XPAttrMerge); 
  return( 1 );
}

/* Set document resolution 
 * Retun error if printer does not support setting a resolution
 */
int XpuSetDocResolution( Display *pdpy, XPContext pcontext, XpuResolutionRec *rec )
{
  if( (XpuGetSupportedDocAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION) == 0 )
    return( 0 );
    
  return XpuSetResolution(pdpy, pcontext, XPDocAttr, rec);
}

/* Set page medium size 
 * Retun error if printer does not support setting a resolution or if per-page
 * resolution changes are not allowed.
 */
int XpuSetPageResolution( Display *pdpy, XPContext pcontext, XpuResolutionRec *rec )
{
  if( (XpuGetSupportedPageAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION) == 0 )
    return( 0 );
    
  return XpuSetResolution(pdpy, pcontext, XPPageAttr, rec);
}

XpuOrientationList XpuGetOrientationList( Display *pdpy, XPContext pcontext, int *numEntriesPtr )
{
  XpuOrientationList list = NULL;
  int                rec_count = 1; /* Allocate one more |XpuOrientationRec|
                                     * structure as terminator */
  char              *value;
  char              *tok_lasts;
  const char        *s;
  const char        *default_orientation = NULL;
  int                default_orientation_rec_index = -1;

  /* Get default document orientation */
  default_orientation = XpGetOneAttribute(pdpy, pcontext, XPDocAttr, "content-orientation"); 
  if( !default_orientation )
  {
    fprintf(stderr, "XpuGetOrientationList: Internal error, no 'content-orientation' XPDocAttr found.\n");
    return(NULL);
  }
  
  value = XpGetOneAttribute(pdpy, pcontext, XPPrinterAttr, "content-orientations-supported");
  if (!value)
  {
    fprintf(stderr, "XpuGetOrientationList: Internal error, no 'content-orientations-supported' XPPrinterAttr found.\n");
    return(NULL);
  }
  
  for( s = strtok_r(value, " ", &tok_lasts) ;
       s != NULL ;
       s = strtok_r(NULL, " ", &tok_lasts) )
  { 
    rec_count++;
    list = (XpuOrientationRec *)realloc(list, sizeof(XpuOrientationRec)*rec_count);
    if( !list )
      return(NULL);
    
    list[rec_count-2].orientation = strdup(s);

    /* Default resolution ? */
    if( !strcmp(list[rec_count-2].orientation, default_orientation) )
    {
      default_orientation_rec_index = rec_count-2;
    }
  }  

  XFree(value);
  XFree((void *)default_orientation);

  if( list )
  {
    /* users: DO NOT COUNT ON THIS DETAIL 
     * (this is only to make current impl. of XpuFreeOrientationList() easier)
     * I may remove this implementation detail in a later revision of
     * the library! */
    list[rec_count-1].orientation = NULL;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }

  /* Make the default orientation always the first item in the list... */
  if( (default_orientation_rec_index != -1) && list )
  {
    XpuOrientationRec tmp;
    tmp = list[0];
    list[0] = list[default_orientation_rec_index];
    list[default_orientation_rec_index] = tmp;
  }

  *numEntriesPtr = rec_count; 
  return(list);
}

void XpuFreeOrientationList( XpuOrientationList list )
{
  if( list )
  {
    XpuOrientationRec *curr = list;
  
    /* See the warning abouve about using this implementation detail for
     * checking for the list's end... */
    while( curr->orientation != NULL )
    {
      free((void *)curr->orientation);
      curr++;
    }   
    free(list);
  }
}

XpuOrientationRec *
XpuFindOrientationByName( XpuOrientationList list, int list_count, const char *orientation )
{
  int i;
  
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuOrientationRec *curr = &list[i];
    if (!strcasecmp(curr->orientation, orientation))
      return curr;
  }

  return(NULL);
}

static
int XpuSetOrientation( Display *pdpy, XPContext pcontext, XPAttributes type, XpuOrientationRec *rec )
{
  XpuSetOneAttribute(pdpy, pcontext, type, "*content-orientation", rec->orientation, XPAttrMerge);
  return(1);
}

/* Set document orientation 
 * Retun error if printer does not support setting an orientation
 */
int XpuSetDocOrientation( Display *pdpy, XPContext pcontext, XpuOrientationRec *rec )
{
  if( (XpuGetSupportedDocAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION) == 0 )
    return( 0 );
    
  return XpuSetOrientation(pdpy, pcontext, XPDocAttr, rec);
}

/* Set page orientation
 * Retun error if printer does not support setting an orientation or if
 * per-page orientations changes are not allowed
 */
int XpuSetPageOrientation( Display *pdpy, XPContext pcontext, XpuOrientationRec *rec )
{
  if( (XpuGetSupportedPageAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION) == 0 )
    return( 0 );
    
  return XpuSetOrientation(pdpy, pcontext, XPPageAttr, rec);
}

XpuPlexList XpuGetPlexList( Display *pdpy, XPContext pcontext, int *numEntriesPtr )
{
  XpuPlexList  list = NULL;
  int          rec_count = 1; /* Allocate one more |XpuPlexList| structure
                               * as terminator */
  char        *value;
  char        *tok_lasts;
  const char  *s;
  const char  *default_plex = NULL;
  int          default_plex_rec_index = -1;

  /* Get default document plex */
  default_plex = XpGetOneAttribute(pdpy, pcontext, XPDocAttr, "plex"); 
  if( !default_plex )
  {
    fprintf(stderr, "XpuGetPlexList: Internal error, no 'plex' XPDocAttr found.\n");
    return(NULL);
  }
   
  value = XpGetOneAttribute(pdpy, pcontext, XPPrinterAttr, "plexes-supported");
  if (!value)
  {
    fprintf(stderr, "XpuGetPlexList: Internal error, no 'plexes-supported' XPPrinterAttr found.\n");
    return(NULL);
  }
  
  for( s = strtok_r(value, " ", &tok_lasts) ;
       s != NULL ;
       s = strtok_r(NULL, " ", &tok_lasts) )
  { 
    rec_count++;
    list = (XpuPlexRec *)realloc(list, sizeof(XpuPlexRec)*rec_count);
    if( !list )
      return(NULL);
    
    list[rec_count-2].plex = strdup(s);

    /* Default plex ? */
    if( !strcmp(list[rec_count-2].plex, default_plex) )
    {
      default_plex_rec_index = rec_count-2;
    }
  }  

  XFree(value);
  XFree((void *)default_plex);

  if( list )
  {
    /* users: DO NOT COUNT ON THIS DETAIL 
     * (this is only to make current impl. of XpuFreePlexList() easier)
     * I may remove this implementation detail in a later revision of
     * the library! */
    list[rec_count-1].plex = NULL;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }

  /* Make the default plex always the first item in the list... */
  if( (default_plex_rec_index != -1) && list )
  {
    XpuPlexRec tmp;
    tmp = list[0];
    list[0] = list[default_plex_rec_index];
    list[default_plex_rec_index] = tmp;
  }

  *numEntriesPtr = rec_count; 
  return(list);
}

void XpuFreePlexList( XpuPlexList list )
{
  if( list )
  {
    XpuPlexRec *curr = list;
  
    /* See the warning abouve about using this implementation detail for
     * checking for the list's end... */
    while( curr->plex != NULL )
    {
      free((void *)curr->plex);
      curr++;
    }   
    free(list);
  }
}

XpuPlexRec *
XpuFindPlexByName( XpuPlexList list, int list_count, const char *plex )
{
  int i;
  
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuPlexRec *curr = &list[i];
    if (!strcasecmp(curr->plex, plex))
      return curr;
  }

  return(NULL);
}

static
int XpuSetContentPlex( Display *pdpy, XPContext pcontext, XPAttributes type, XpuPlexRec *rec )
{
  XpuSetOneAttribute(pdpy, pcontext, type, "*plex", rec->plex, XPAttrMerge);
  return(1);
}

/* Set document plex 
 * Retun error if printer does not support setting an plex
 */
int XpuSetDocPlex( Display *pdpy, XPContext pcontext, XpuPlexRec *rec )
{
  if( (XpuGetSupportedDocAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_PLEX) == 0 )
    return( 0 );
    
  return XpuSetContentPlex(pdpy, pcontext, XPDocAttr, rec);
}

/* Set page plex
 * Retun error if printer does not support setting an plex or if
 * per-page plex changes are not allowed
 */
int XpuSetPagePlex( Display *pdpy, XPContext pcontext, XpuPlexRec *rec )
{
  if( (XpuGetSupportedPageAttributes(pdpy, pcontext) & XPUATTRIBUTESUPPORTED_PLEX) == 0 )
    return( 0 );
    
  return XpuSetContentPlex(pdpy, pcontext, XPPageAttr, rec);
}


XpuColorspaceList XpuGetColorspaceList( Display *pdpy, XPContext pcontext, int *numEntriesPtr )
{
  XpuColorspaceList list = NULL;
  int               rec_count = 1; /* Allocate one more |XpuColorspaceRec| structure
                                    * as terminator */
  char              namebuf[256];  /* Temporary name buffer for colorspace names */
  int               i;             /* Loop counter */
  int               nvi;           /* Number of visuals */
  Screen           *pscreen;       /* Print screen */
  XVisualInfo       viproto;       /* fill in for getting info */
  XVisualInfo      *vip;           /* retured info */

  pscreen = XpGetScreenOfContext(pdpy, pcontext);

  nvi = 0;
  viproto.screen = XScreenNumberOfScreen(pscreen);
  vip = XGetVisualInfo(pdpy, VisualScreenMask, &viproto, &nvi);
  if (!vip)
  {
    fprintf(stderr, "XpuGetColorspaceList: Internal error: vip == NULL\n");
    return NULL;
  }
  
  for( i = 0 ; i < nvi ; i++ )
  {
    XVisualInfo *vcurr = vip+i;
    char         cbuff[64];
    const char  *class = NULL;

#ifdef USE_MOZILLA_TYPES
    /* Workaround for the current limitation of the gfx/src/xlibrgb code
     * which cannot handle depths > 24bit yet */
    if( vcurr->depth > 24 )
      continue;
#endif /* USE_MOZILLA_TYPES */
 
    rec_count++;
    list = (XpuColorspaceRec *)realloc(list, sizeof(XpuColorspaceRec)*rec_count);
    if( !list )
      return NULL;

    /* ToDO: This needs to be updated for the COLORSPACE X11 extension
     * once it is ready and approved by the XOrg arch board. */
    switch (vcurr->class) {
      case StaticGray:   class = "StaticGray";  break;
      case GrayScale:    class = "GrayScale";   break;
      case StaticColor:  class = "StaticColor"; break;
      case PseudoColor:  class = "PseudoColor"; break;
      case TrueColor:    class = "TrueColor";   break;
      case DirectColor:  class = "DirectColor"; break;
      default: /* Needed for forward compatibility to the COLORSPACE extension */
        sprintf (cbuff, "unknown_class_%x", vcurr->class);
        class = cbuff;
        break;
    }

    if (vcurr->bits_per_rgb == 8)
    {
      sprintf(namebuf, "%s/%dbit", class, vcurr->depth);
    }
    else
    {
      sprintf(namebuf, "%s/%dbit/%dbpg", class, vcurr->depth, vcurr->bits_per_rgb);
    }
    list[rec_count-2].name       = strdup(namebuf);
    list[rec_count-2].visualinfo = *vcurr;
  }  
 
  XFree((char *)vip);

  if( list )
  {
    /* users: DO NOT COUNT ON THIS DETAIL 
     * (this is only to make current impl. of XpuGetResolutionList() easier)
     * We may remove this implementation detail in a later revision of
     * the library! */
    list[rec_count-1].name = NULL;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }

  *numEntriesPtr = rec_count; 
  return(list);
}

void XpuFreeColorspaceList( XpuColorspaceList list )
{
  if( list )
  { 
    XpuColorspaceRec *curr = list;
  
    /* See the warning abouve about using this implementation detail for
     * checking for the list's end... */
    while( curr->name != NULL )
    {
      free((void *)curr->name);
      curr++;
    }  

    free(list);
  }
}

XpuColorspaceRec *
XpuFindColorspaceByName( XpuColorspaceList list, int list_count, const char *name )
{
  int i;
  
  for( i = 0 ; i < list_count ; i++ )
  {
    XpuColorspaceRec *curr = &list[i];
    if (!strcmp(curr->name, name))
      return curr;
  }

  return(NULL);
}

Bool XpuGetEnableFontDownload( Display *pdpy, XPContext pcontext )
{
  Bool  enableFontDownload;
  char *value;
  
  value = XpGetOneAttribute(pdpy, pcontext, XPPrinterAttr, "xp-listfonts-modes-supported"); 
  if( !value )
  {
    fprintf(stderr, "XpuGetEnableFontDownload: xp-listfonts-modes-supported printer attribute not found.\n");
    return False;
  }
  
  enableFontDownload = (strstr(value, "xp-list-glyph-fonts") != NULL);
  XFree(value);
  return enableFontDownload;
}

int XpuSetEnableFontDownload( Display *pdpy, XPContext pcontext, Bool enableFontDownload )
{
  char *value,
       *newvalue;
  
  value = XpGetOneAttribute(pdpy, pcontext, XPPrinterAttr, "xp-listfonts-modes-supported"); 
  if( !value )
  {
    fprintf(stderr, "XpuSetEnableFontDownload: xp-listfonts-modes-supported printer attribute not found.\n");
    return 0; /* failure */
  }
  
  /* Set "xp-list-glyph-fonts" */
  if( enableFontDownload )
  {
    /* Return success if "xp-list-glyph-fonts" is already set */
    if( strstr(value, "xp-list-glyph-fonts") != NULL )
    {
      XFree(value);
      return 1; /* success */
    }

    newvalue = malloc(strlen(value) + 33);
    if( !newvalue )
    {
      XFree(value);
      return 0; /* failure */
    }

    sprintf(newvalue, "%s xp-list-glyph-fonts", value);
    XpuSetOneAttribute(pdpy, pcontext, XPDocAttr, "*xp-listfonts-modes", newvalue, XPAttrMerge);

    free(newvalue);
    XFree(value);
    return 1; /* success */
  }
  else
  {
    char *s, /* copy string "source" */
         *d; /* copy string "destination" */
    
    /* Return success if "xp-list-glyph-fonts" not set */
    d = strstr(value, "xp-list-glyph-fonts");
    if( d == NULL )
    {
      XFree(value);
      return 1; /* success */
    }

    /* strip "xp-list-glyph-fonts" from |value| */
    s = d+19/*strlen("xp-list-glyph-fonts")*/;
    while( (*d++ = *s++) != '\0' )
      ;

    XpuSetOneAttribute(pdpy, pcontext, XPDocAttr, "*xp-listfonts-modes", value, XPAttrMerge);

    XFree(value);
    return 1; /* success */
  } 
}

/* Return flags to indicate which attributes are supported and which not... */
static
XpuSupportedFlags XpuGetSupportedAttributes( Display *pdpy, XPContext pcontext, XPAttributes type, const char *attribute_name )
{
  char              *value;
  void              *tok_lasts;
  XpuSupportedFlags  flags = 0;
  
  MAKE_STRING_WRITABLE(attribute_name);
  if( attribute_name == NULL )
    return(0);
    
  value = XpGetOneAttribute(pdpy, pcontext, type, STRING_AS_WRITABLE(attribute_name));   
  
  FREE_WRITABLE_STRING(attribute_name);
  
  if( value != NULL )
  {
    const char *s;
    
    for( s = XpuEnumerateXpAttributeValue(value, &tok_lasts) ; s != NULL ; s = XpuEnumerateXpAttributeValue(NULL, &tok_lasts) )
    {
           if( !strcmp(s, "job-name") )                   flags |= XPUATTRIBUTESUPPORTED_JOB_NAME;
      else if( !strcmp(s, "job-owner") )                  flags |= XPUATTRIBUTESUPPORTED_JOB_OWNER;
      else if( !strcmp(s, "notification-profile") )       flags |= XPUATTRIBUTESUPPORTED_NOTIFICATION_PROFILE;
      else if( !strcmp(s, "copy-count") )                 flags |= XPUATTRIBUTESUPPORTED_COPY_COUNT;
      else if( !strcmp(s, "document-format") )            flags |= XPUATTRIBUTESUPPORTED_DOCUMENT_FORMAT;
      else if( !strcmp(s, "content-orientation") )        flags |= XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION;
      else if( !strcmp(s, "default-printer-resolution") ) flags |= XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION;
      else if( !strcmp(s, "default-input-tray") )         flags |= XPUATTRIBUTESUPPORTED_DEFAULT_INPUT_TRAY;
      else if( !strcmp(s, "default-medium") )             flags |= XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM;
      else if( !strcmp(s, "plex") )                       flags |= XPUATTRIBUTESUPPORTED_PLEX;
      else if( !strcmp(s, "xp-listfonts-modes") )         flags |= XPUATTRIBUTESUPPORTED_LISTFONTS_MODES;
    }
    
    XpuDisposeEnumerateXpAttributeValue(&tok_lasts);
    XFree(value);
  }  
  
  return(flags);
}

XpuSupportedFlags XpuGetSupportedJobAttributes(Display *pdpy, XPContext pcontext)
{
  return XpuGetSupportedAttributes(pdpy, pcontext, XPPrinterAttr, "job-attributes-supported");
}

XpuSupportedFlags XpuGetSupportedDocAttributes(Display *pdpy, XPContext pcontext)
{
  return XpuGetSupportedAttributes(pdpy, pcontext, XPPrinterAttr, "document-attributes-supported");
}

XpuSupportedFlags XpuGetSupportedPageAttributes(Display *pdpy, XPContext pcontext)
{
  return XpuGetSupportedAttributes(pdpy, pcontext, XPPrinterAttr, "xp-page-attributes-supported");
}

/* Encode  string  for  usage  in a Xrm  resource  database  as
 * defined  in  X(7):  [...]  To  allow  a  Value  to  begin
 * with  whitespace,  the  two-character  sequence   ``\space''
 * (backslash  followed by space) is recognized and replaced by
 * a space character, and the two-character  sequence  ``\tab''
 * (backslash  followed  by  horizontal  tab) is recognized and
 * replaced by a horizontal tab character.  To allow a Value to
 * contain   embedded  newline  characters,  the  two-character
 * sequence ``\n'' is recognized  and  replaced  by  a  newline
 * character.   To  allow  a Value to be broken across multiple
 * lines in a text file,  the  two-character  sequence  ``\new-
 * line''  (backslash  followed  by  newline) is recognized and
 * removed from the value.  To allow a Value to  contain  arbi-
 * trary character codes, the four-character sequence ``\nnn'',
 * where  each  n  is  a  digit  character  in  the  range   of
 * ``0''-``7'',  is  recognized and replaced with a single byte
 * that contains the octal value  specified  by  the  sequence.
 * Finally, the two-character sequence ``\\'' is recognized and
 * replaced with a single backslash.
 */
char *XpuResourceEncode( const char *s )
{
  size_t  slen;
  char   *res;
  char   *d;
  int     i,
          c;

  slen = strlen(s);
  res  = malloc(slen*4+1);
  if (!res)
    return NULL;
  
  d = res;
  i = slen;
  while (i--) {
    c = *s++;
    if (c == '\n') {
      if (i) {
        *d++ = '\\';
        *d++ = 'n';
        *d++ = '\\';
        *d++ = '\n';
      }
      else {
        *d++ = '\\';
        *d++ = 'n';
      }
    } else if (c == '\\') {
        *d++ = '\\';
        *d++ = '\\';
    }
    else if ((c < ' ' && c != '\t') ||
            ((unsigned char)c >= 0x7F && (unsigned char)c < 0xA0)) {
        sprintf(d, "\\%03o", (unsigned char)c);
        d += 4;
    }
    else {
        *d++ = c;
    }
  }

  *d = '\0';
  
  return res;
}

#ifdef XXXJULIEN_NOTNOW
char *XpuResourceDecode( const char *str )
{
}
#endif /* XXXJULIEN_NOTNOW */

void XpuResourceFreeString( char *s )
{
  free(s);
}

const char *XpuXmbToCompoundText(Display *dpy, const char *xmbtext)
{
  XTextProperty   xtp;
  int             xcr;
  char           *xtl[2];
  char           *ct;

  if (strlen(xmbtext) == 0)
    return strdup(xmbtext);
  
  memset(&xtp, 0, sizeof(xtp));
  xtl[0] = (char *)xmbtext;
  xtl[1] = NULL;
  
  xcr = XmbTextListToTextProperty(dpy, xtl, 1, XCompoundTextStyle, &xtp);
  
  if (xcr == XNoMemory || xcr == XLocaleNotSupported)
  {
    fprintf(stderr, "XpuXmbToCompoundText: XmbTextListToTextProperty failure.\n");
    return strdup(xmbtext);
  }

  /* Did conversion succeed (some unconvertible characters
   * are not a problem) ? */
  if ( !((xcr == Success) || (xcr > 0)) ||
       (xtp.value == NULL))
  {
    fprintf(stderr, "XpuXmbToCompoundText: XmbTextListToTextProperty failure 2.\n");
    return strdup(xmbtext);
  }
  
  ct = malloc(xtp.nitems+1);
  if (!ct)
  {
    XFree(xtp.value);
    return NULL;
  }
  memcpy(ct, xtp.value, xtp.nitems);
  ct[xtp.nitems] = '\0';  

  XFree(xtp.value);
  
  return ct;
}

void XpuFreeCompundTextString( const char *s )
{
  free((void *)s);
}

const char *XpuCompoundTextToXmb(Display *dpy, const char *ct)
{
  XTextProperty   xtp;
  int             xcr;
  char          **xtl = NULL;
  int             xtl_count = 0;
  char           *xmb;
  int             xmb_len = 0;
  int             i;

  if (strlen(ct) == 0)
    return strdup(ct);
    
  xtp.value    = (unsigned char *)ct;
  xtp.nitems   = strlen(ct); 
  xtp.encoding = XInternAtom(dpy, "COMPOUND_TEXT", False);
  xtp.format   = 8;
  
  xcr = XmbTextPropertyToTextList(dpy, &xtp, &xtl, &xtl_count);
  
  if (xcr == XNoMemory || xcr == XLocaleNotSupported)
  {
    fprintf(stderr, "XpuCompoundTextToXmb: XmbTextPropertyToTextList failure 1.\n");
    return strdup(ct);
  }

  /* Did conversion succeed (some unconvertible characters
   * are not a problem) ? */
  if ( !((xcr == Success) || (xcr > 0)) ||
       (xtl == NULL))
  {
    fprintf(stderr, "XpuCompoundTextToXmb: XmbTextPropertyToTextList failure 2.\n");
    return strdup(ct);
  }
   
  for (i = 0; i < xtl_count; i++)
  {
    xmb_len += strlen(xtl[i]);
  }
  xmb = malloc (xmb_len + 1);
  if (!xmb)
  {
    XFreeStringList(xtl);
    return NULL;
  }
  xmb[0] = '\0'; /* Catch zero-length case */
  for (i = 0; i < xtl_count; i++)
  {
    strcat(xmb, xtl[i]);
  }
  
  XFreeStringList(xtl); 
  
  return xmb;
}

void XpuFreeXmbString( const char *s )
{
  free((void *)s);
}

/* EOF. */
