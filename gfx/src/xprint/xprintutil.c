/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
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
 * The Original Code is the X11 print system utilities library.
 * 
 * The Initial Developer of the Original Code is Roland Mainz 
 * <roland.mainz@informatik.med.uni-giessen.de>.
 * All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "xprintutil.h"
 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

/* conformace only; X11 API does (currrently) not make use of |const|. 
 * If Xlib API gets fixed these macros can be turned into empty 
 * placeholders... (|#define MAKE_STRING_WRITABLE(x)|) :-)
 */
#define MAKE_STRING_WRITABLE(str) (((str)?((str) = strdup(str)):0))
#define FREE_WRITABLE_STRING(str) free((void *)(str))
#define STRING_AS_WRITABLE(str) ((char *)(str))

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


const char *XpuGetXpServerList( void )
{
  /* BUG/TODO: XpServerList resource needs to be sourced first, then append 
   * contents of XPSERVERLIST, then remove duplicates...
   */
  return(getenv("XPSERVERLIST"));
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
        for( display = strtok_r(sl, " ", &tok_lasts) ; 
             display != NULL ; 
             display = strtok_r(NULL, " ", &tok_lasts) )
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


void XpuSetOneAttribute( Display *pdpy, XPContext pcontext, 
                         XPAttributes type, const char *attribute_name, const char *value, XPAttrReplacement replacement_rule )
{
  /* Alloc buffer for sprintf() stuff below */
  char *buffer = malloc(strlen(attribute_name)+strlen(value)+4);
  
  if( buffer != NULL )
  {
    sprintf(buffer, "%s: %s", attribute_name, value);      
    XpSetAttributes(pdpy, pcontext, type, buffer, replacement_rule);
    free(buffer);
  }  
}

/* check if attribute value is supported or not */
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
    
    for( s = XpuEmumerateXpAttributeValue(value, &tok_lasts) ; s != NULL ; s = XpuEmumerateXpAttributeValue(NULL, &tok_lasts) )
    {
      XPU_DEBUG_ONLY(printf("XpuCheckSupported: probing '%s'=='%s'\n", XPU_NULLXSTR(s), XPU_NULLXSTR(query)));
      if( !strcmp(s, query) )
      {
        XFree(value);
        XpuDisposeEmumerateXpAttributeValue(&tok_lasts);
        return(1);
      }  
    }
    
    XpuDisposeEmumerateXpAttributeValue(&tok_lasts);
    XFree(value);
  }  
  
  return(0);
}


int XpuSetJobTitle( Display *pdpy, XPContext pcontext, const char *title )
{
  if( XpuCheckSupported(pdpy, pcontext, XPPrinterAttr, "job-attributes-supported", "job-name") )
  {
    XpuSetOneAttribute(pdpy, pcontext, XPJobAttr, "*job-name", title, XPAttrMerge);
    return(1);
  }
  else
  {
    XPU_DEBUG_ONLY(printf("XpuSetJobTitle: XpuCheckSupported failed for '%s'\n", XPU_NULLXSTR(title)));
    return(0); 
  }  
}
    
        
int XpuSetContentOrientation( Display *pdpy, XPContext pcontext, XPAttributes type, const char *orientation )
{
  /* fixme: check whether the given |orientation| is supported or not... */
  if( XpuCheckSupported(pdpy, pcontext, XPPrinterAttr, "content-orientations-supported", orientation) )
  {
    XpuSetOneAttribute(pdpy, pcontext, type, "*content-orientation", orientation, XPAttrMerge);
    return(1);
  }
  else
  {
    XPU_DEBUG_ONLY(printf("XpuSetContentOrientation: XpuCheckSupported failed for '%s'\n", XPU_NULLXSTR(orientation)));
    return(0);
  }  
}

/* ToDo: Implement
const char *XpuGetContentOrientation( Display *pdpy, XPContext pcontext, XPAttributes type );
*/

/* ToDo:
.* XpuGetPlex(), XpuSetPlex()
 */

int XpuGetOneLongAttribute( Display *pdpy, XPContext pcontext, XPAttributes type, const char *attribute_name, long *result )
{
  char *s;
  
  MAKE_STRING_WRITABLE(attribute_name);
  if( attribute_name == NULL )
    return(0);
  s = XpGetOneAttribute(pdpy, pcontext, type, STRING_AS_WRITABLE(attribute_name));
  
  if( (s != NULL) && (strlen(s) > 0) ) 
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
  
  XPU_DEBUG_ONLY(puts("XpuGetOneLongAttribute failed\n"));
  FREE_WRITABLE_STRING(attribute_name);
  
  return(0);
}


#ifdef DEBUG
/* debug only */
void dumpXpAttributes( Display *pdpy, XPContext pcontext )
{
  /* BUG: values from XpuGet*Attributes should be passed to XFree() after use... :-) */
  printf("------------------------------------------------\n");
  printf("--> Job\n%s\n",     XpuGetJobAttributes(pdpy, pcontext));
  printf("--> Doc\n%s\n",     XpuGetDocAttributes(pdpy, pcontext));
  printf("--> Page\n%s\n",    XpuGetPageAttributes(pdpy, pcontext));
  printf("--> Printer\n%s\n", XpuGetPrinterAttributes(pdpy, pcontext));
  printf("--> Server\n%s\n",  XpuGetServerAttributes(pdpy, pcontext));
  printf("image resolution %d\n", (int)XpGetImageResolution(pdpy, pcontext));
  printf("------------------------------------------------\n");
}
#endif /* DEBUG */    


/* BUG: Is it really neccesary that this function eats-up all other events ? */
void XpuWaitForPrintNotify( Display *pdpy, int detail )
{
  /* Xprt |Display *| for which "event_base_return" and "error_base_return" are valid  */
  static Display *ext_display = NULL; 
  static int      event_base_return = -1, 
                  error_base_return = -1;
         XEvent   ev;
  
  /* get extension event_base if we did not get it yet (and if Xserver does not 
   * support extension do not wait for events which will never be send... :-) 
   */
  if( ((event_base_return == -1) && (error_base_return == -1)) || (ext_display != pdpy) )
  {
    int myevent_base_return, myerror_base_return;
    
    if( XpQueryExtension(pdpy, &myevent_base_return, &myerror_base_return) == False )
    {
      XPU_DEBUG_ONLY(printf("XpuWaitForPrintNotify: XpQueryExtension failed\n"));
      return;
    }
    
    /* be sure we don't get in trouble if two threads try the same thing :-)
     */
    event_base_return = myevent_base_return;
    error_base_return = myerror_base_return;
    ext_display = pdpy;
  }
  
  do 
  {
    XNextEvent(pdpy, &ev);
    if( ev.type != (event_base_return+XPPrintNotify) )
    {
      XPU_DEBUG_ONLY(printf("XpuWaitForPrintNotify: Killing non-PrintNotify event %d/%d while waiting for %d/%d\n", 
               (int)ev.type, (int)((XPPrintEvent *)(&ev))->detail, 
               (int)(event_base_return+XPPrintNotify), detail));
    }
  } while( !((ev.type == (event_base_return+XPPrintNotify)) && (((XPPrintEvent *)(&ev))->detail == detail)) );
}      

/* set print resolution
 * Retun error if printer does not support this resolution
 */
Bool XpuSetResolution( Display *pdpy, XPContext pcontext, long dpi )
{
  /* not implemented yet */
  return False;
}

/* get default printer reolution
 * this function may fail in the following conditions:
 * - Xprt misconfiguration
 * - X DPI != Y DPI (not yet implemented in Xprt)
 */
Bool XpuGetResolution( Display *pdpy, XPContext pcontext, long *dpi_ptr )
{
  if( XpuGetOneLongAttribute(pdpy, pcontext, XPDocAttr, "default-printer-resolution", dpi_ptr) == 1 )
  {
    return True;
  }

  return False;
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

/* PRIVATE context data for XpuEmumerateXpAttributeValue() */
typedef struct _XpuAttributeValueEnumeration
{
  char *value;
  char *start;
  char *s;
} XpuAttributeValueEnumeration;

const char *XpuEmumerateXpAttributeValue( const char *value, void **vcptr )
{
  XpuAttributeValueEnumeration **cptr = (XpuAttributeValueEnumeration **)vcptr;
  XpuAttributeValueEnumeration  *context;
  
  if( !cptr )
    return(NULL);
    
  if( value )
  {
    XpuAttributeValueEnumeration *e;
    const char *s = value;
  
    e = malloc(sizeof(XpuAttributeValueEnumeration));
    if( !e )
      return NULL;
  
    /* skip leading '{'. */
    while(*s=='{')
      s++;
    /* skip leading blanks or '\''. */
    while(isspace(*s) || *s=='\'')
      s++;
       
    e->start = e->s = e->value = strdup(s);    
  
    *cptr = e;
  }
  
  context = *cptr;
  
  if( !context || !context->s )
    return(NULL);
   
  /* Skip leading blanks, '\'' or '}' */
  while(isspace(*(context->s)) || *(context->s)=='\'' || *(context->s)=='}' )
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
  
  return(context->start);   
}


void XpuDisposeEmumerateXpAttributeValue( void **vc )
{ 
  if( vc )
  {
    XpuAttributeValueEnumeration *context = *((XpuAttributeValueEnumeration **)vc);
    free(context->value);
    free(context);   
  }
}

/* parse a paper size string 
 * (example: '{na-letter False {6.3500 209.5500 6.3500 273.0500}}') */
Bool XpuParseMediumSourceSize( const char *value, 
                               const char **media_name, Bool *mbool, 
                               float *ma1, float *ma2, float *ma3, float *ma4 )
{
  const char *s;
  char       *d, 
             *name;
  char       *boolbuf;
  size_t      value_len;
  
  if( value && value[0]!='{' && value[0]!='\0' )
    return(False);
    
  value_len = strlen(value);
  
  /* alloc buffer for "media_name" and |boolbuf| in one step
   * (both must be large enougth to hold at least |strlen(value)+1| bytes) */  
  name = malloc(value_len*2 + 4);
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
    
  /* seperate media name from string */
  d = (char *)search_next_space(name);
  if( !d )
  {
    free(name);
    return(False);
  }  
  *d = '\0';
  *media_name = name;
  
  /* ... continue to parse the remaining string... */
  d++;
  
  if( sscanf(d, "%s %f %f %f %f", boolbuf, ma1, ma2, ma3, ma4) != 5 )
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

/* future: Migrate this functionality into |XpGetPrinterList| - just do
 * not pass a |Display *| to |XpGetPrinterList|
 */
XPPrinterList XpuGetPrinterList( const char *printer, int *res_list_count )
{
  XPPrinterRec *rec = NULL;
  int           rec_count = 1; /* allocate one more XPPrinterRec structure
                                * as terminator */
  char         *sl;
  
  if( !res_list_count )
    return(NULL);
  
  sl = strdup(XpuGetXpServerList());
  MAKE_STRING_WRITABLE(printer);
    
  if( sl != NULL )
  {
    char *display;
    char *tok_lasts;
    
    for( display = strtok_r(sl, " ", &tok_lasts) ; 
         display != NULL ; 
         display = strtok_r(NULL, " ", &tok_lasts) )
    {
      Display *pdpy;
      
      if( pdpy = XOpenDisplay(display) )
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
            rec_count++;             
            rec = realloc(rec, sizeof(XPPrinterRec)*rec_count);
            if( !rec ) /* failure */
              break;
              
            s = malloc(strlen(list[i].name)+display_len+4);
            sprintf(s, "%s@%s", list[i].name, display);
            rec[rec_count-2].name = s;
            rec[rec_count-2].desc = (list[i].desc)?(strdup(list[i].desc)):(NULL);
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
     * (this is only to make current impl. of XpuFreePrinterList() easier) */
    rec[rec_count-1].name = NULL;
    rec[rec_count-1].desc = NULL;
    rec_count--;
  }
  else
  {
    rec_count = 0;
  }
    
  *res_list_count = rec_count;
  FREE_WRITABLE_STRING(printer);
  return(rec);
}      


void XpuFreePrinterList( XPPrinterList list )
{
  if( list )
  {
    XPPrinterList curr = list;
  
    while(curr->name!=NULL)
    {
      free(curr->name);
      free(curr->desc);
      curr++;
    }
  
    free(list);
  }
}

/* EOF. */
