
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
 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#define NeedFunctionPrototypes (1) /* is this legal from within an app. !? */
#include <X11/Xlibint.h>
#include <X11/extensions/Print.h>
#include <X11/Intrinsic.h>

#include "xprintutil.h"

/*
** XprintUtil functions start with Xpu
**
*/

int XpuCheckExtension( Display *pdpy )
{
    char *display = DisplayString(pdpy);
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


char *XpuGetXpServerList( void )
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
          if( (pcontext = XpCreateContext(pdpy, printer)) != NULL )
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
int XpuGetPrinter( char *printername, Display **pdpyptr, XPContext *pcontextptr )
{
    Display       *pdpy;
    XPContext      pcontext;
    char          *s;
    char          *tok_lasts;
    
    *pdpyptr     = NULL;
    *pcontextptr = NULL;
    
    XPU_DEBUG_ONLY(printf("XpuGetPrinter: looking for '%s'\n", XPU_NULLXSTR(printername)));
    
    /* strtok_r will modify string - duplicate it first... */
    printername = (char *)strdup(printername);
    
    if( (s = (char *)strtok_r(printername, "@", &tok_lasts)) != NULL )
    {
      char *name = s;
      char *display = (char *)strtok_r(NULL, "@", &tok_lasts);
      
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
        char *sl = XpuGetXpServerList();
        
        if( sl != NULL )
        {
          for( display = (char *)strtok_r(sl, " ", &tok_lasts) ; 
               display != NULL ; 
               display = (char *)strtok_r(NULL, " ", &tok_lasts) )
          {
            if( XpuGetPrinter2(name, display, pdpyptr, pcontextptr) )
            {
              free(printername);
              return(1);
            } 
          }
        }  
      }
    }
    
    free(printername);
    XPU_DEBUG_ONLY(printf("XpuGetPrinter: failure\n"));
  
    return(0);
}


void XpuSetOneAttribute( Display *pdpy, XPContext pcontext, 
                         XPAttributes type, char *attribute_name, char *value, XPAttrReplacement replacement_rule )
{
    char *buffer = 
#ifdef XPU_USE_NSPR
    PR_Malloc((PRUint32)
#else    
    Xmalloc(
#endif
    strlen(attribute_name)+strlen(value)+8);
    
    if( buffer != NULL )
    {
      sprintf(buffer, "%s: %s", attribute_name, value);      
      XpSetAttributes(pdpy, pcontext, type, buffer, replacement_rule);
#ifdef XPU_USE_NSPR
      PR_Free
#else
      XFree
#endif      
      (buffer);
    }  
}


/* enumerate attribute values - work in porgress */
char *XpuEmumerateXpAttributeValue( char *value, void **context )
{
    /* BUG: This does not work for quoted attribute values
     * A good example is the enumeration of supported paper sizes: 
     * "{'' {na-letter False {6.3500 209.5500 6.3500 273.0500}} {executive False {6.3500 177.7500 6.3500 260.3500}} {na-legal False {6.3500 209.5500 6.3500 349.2500}} {iso-a4 False {6.3500 203.6500 6.3500 290.6500}} {iso-designated-long False {6.3500 103.6500 6.3500 213.6500}} {na-number-10-envelope False {6.3500 98.4500 6.3500 234.9500}} }" 
      {'' 
         {  
           na-letter 
           False 
           {6.3500 209.5500 6.3500 273.0500}
         } 
         { 
           executive 
           False 
           {6.3500 177.7500 6.3500 260.3500}
         } 
         {
           na-legal 
           False 
           {6.3500 209.5500 6.3500 349.2500}
         } 
         {
           iso-a4 
           False 
           {6.3500 203.6500 6.3500 290.6500}
         } 
         {
           iso-designated-long 
           False 
           {6.3500 103.6500 6.3500 213.6500}
         } 
         {
           na-number-10-envelope 
           False 
           {6.3500 98.4500 6.3500 234.9500}
         } 
       }
     
     * ToDo: 
     * 1. Write a parser which grabs the 1st level of bracket pairs and 
     * enumerate these items (six items in this example).
     * 2. Write enumeration functions for single attribute types with 
     * complex content.
     */
    return( (char *)strtok_r(value, " ", (char **)context) );
}


/* check if attribute value is supported or not */
int XpuCheckSupported( Display *pdpy, XPContext pcontext, XPAttributes type, char *attribute_name, char *query )
{
    char *value,
         *s;
    void *tok_lasts;
    
    value = XpGetOneAttribute(pdpy, pcontext, type, attribute_name);
    
    XPU_DEBUG_ONLY(printf("XpuCheckSupported: XpGetOneAttribute(%s) returned '%s'\n", XPU_NULLXSTR(attribute_name), XPU_NULLXSTR(value)));
    
    if( value != NULL )
    {
      for( s = XpuEmumerateXpAttributeValue(value, &tok_lasts) ; s != NULL ; s = XpuEmumerateXpAttributeValue(NULL, &tok_lasts) )
      {
        XPU_DEBUG_ONLY(printf("XpuCheckSupported: probing '%s'=='%s'\n", XPU_NULLXSTR(s), XPU_NULLXSTR(query)));
        if( !strcmp(s, query) )
        {
          XFree(value);
          return(1);
        }  
      }
      
      XFree(value);
    }  
    
    return(0);
}


int XpuSetJobTitle( Display *pdpy, XPContext pcontext, char *title )
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
    
        
int XpuSetContentOrientation( Display *pdpy, XPContext pcontext, XPAttributes type, char *orientation )
{
    if( XpuCheckSupported(pdpy, pcontext, XPPrinterAttr, "content-orientations-supported", orientation) )
    {
      XpuSetOneAttribute(pdpy, pcontext, type, "*content-orientation", orientation, XPAttrMerge);
    }
    else
    {
      XPU_DEBUG_ONLY(printf("XpuSetContentOrientation: XpuCheckSupported failed for '%s'\n", XPU_NULLXSTR(orientation)));
      return(0);
    }  
}


int XpuGetOneLongAttribute( Display *pdpy, XPContext pcontext, XPAttributes type, char *attribute_name, long *result )
{
    char *s = XpGetOneAttribute(pdpy, pcontext, type, attribute_name);
    
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
        return(1);
      }            
    }
    
    if( s != NULL ) XFree(s);
    
    puts("XpuGetOneLongAttribute failed\n");
    
    return(0);
}


double XpuGetXDPI( Screen *pscreen )
{
    double xres;
    
    /* from X11R6.5.1/xc/programs/xdpyinfo/xdpyinfo.c:
     * there are 2.54 centimeters to an inch; so there are 25.4 millimeters.
     *
     *     dpi = N pixels / (M millimeters / (25.4 millimeters / 1 inch))
     *         = N pixels / (M inch / 25.4)
     *         = N * 25.4 pixels / M inch
     */

    xres = ((((double) WidthOfScreen(pscreen)) * 25.4) / 
            ((double) WidthMMOfScreen(pscreen)));
           
    return(xres);        
}


double XpuGetYDPI( Screen *pscreen )
{
    double yres;
    
    /* from X11R6.5.1/xc/programs/xdpyinfo/xdpyinfo.c:
     * there are 2.54 centimeters to an inch; so there are 25.4 millimeters.
     *
     *     dpi = N pixels / (M millimeters / (25.4 millimeters / 1 inch))
     *         = N pixels / (M inch / 25.4)
     *         = N * 25.4 pixels / M inch
     */

    yres = ((((double) HeightOfScreen(pscreen)) * 25.4) / 
            ((double) HeightMMOfScreen(pscreen)));
           
    return(yres);        
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
    static int    event_base_return = -1, 
                  error_base_return = -1;
           XEvent ev;
    
    /* get extension event_base if we did not get it yet (and if Xserver does not 
     * support extension do not wait for events which will never be send... :-) 
     */
    if( (event_base_return == -1) && (error_base_return == -1) )
    {
      int myevent_base_return, myerror_base_return;
      
      if( XpQueryExtension(pdpy, &myevent_base_return, &myerror_base_return) == False )
      {
        XPU_DEBUG_ONLY(printf("XpuWaitForPrintNotify: XpQueryExtension failed\n"));
        return;
      }
      
      /* be sure we don't get in trouble if two threads try the same thing :-)
       * Bug/issue: Is it gurantteed that two different Xprt server's return the same values here ??
       */
      event_base_return = myevent_base_return;
      error_base_return = myerror_base_return;
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


/* EOF. */
