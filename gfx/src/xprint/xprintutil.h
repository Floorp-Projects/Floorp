/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef XPRINTUTIL_H
#define XPRINTUTIL_H 1
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

#ifndef NeedFunctionPrototypes 
#define NeedFunctionPrototypes (1) /* is this legal from within an app. !? */
#endif
#include <X11/Xlibint.h>
#include <X11/extensions/Print.h>
#include <X11/Intrinsic.h>

/* I don't know how to make this "better" yet... ;-( */
#ifdef USE_MOZILLA_TYPES
#include <prmem.h>
#include <prthread.h>
#define XPU_USE_NSPR 1
#define XPU_USE_THREADS 1
#endif /* USE_MOZILLA_TYPES */

#ifdef DEBUG
/* trace function calls */
#define XPU_TRACE(EX)  (puts(#EX),EX)
/* trace function calls in child */
#define XPU_TRACE_CHILD(EX)  (puts("child: " #EX),EX)
/* execute function EX only in debug mode */
#define XPU_DEBUG_ONLY(EX)  (EX)
#else
#define XPU_TRACE(EX) (EX)
#define XPU_TRACE_CHILD(EX) (EX)
#define XPU_DEBUG_ONLY(EX)
#endif /* DEBUG */

/* debug: replace NULLptrs with "<NULL>" string */
#define XPU_NULLXSTR(s) (((s)!=NULL)?(s):("<NULL>"))

/* prototypes */
_XFUNCPROTOBEGIN

int XpuCheckExtension( Display *pdpy );
const char *XpuGetXpServerList( void );
int XpuGetPrinter( const char *printername, Display **pdpyptr, XPContext *pcontextptr );
void XpuSetOneAttribute( Display *pdpy, XPContext pcontext, 
                         XPAttributes type, const char *attribute_name, const char *value, XPAttrReplacement replacement_rule );
int XpuCheckSupported( Display *pdpy, XPContext pcontext, XPAttributes type, const char *attribute_name, const char *query );
int XpuSetJobTitle( Display *pdpy, XPContext pcontext, const char *title );
int XpuSetContentOrientation( Display *pdpy, XPContext pcontext, XPAttributes type, const char *orientation );
int XpuGetOneLongAttribute( Display *pdpy, XPContext pcontext, XPAttributes type, const char *attribute_name, long *result );
void dumpXpAttributes( Display *pdpy, XPContext pcontext );
void XpuSetContext( Display *pdpy, XPContext pcontext );
void XpuWaitForPrintNotify( Display *pdpy, int detail );
Bool XpuSetResolution( Display *pdpy, XPContext pcontext, long dpi );
Bool XpuGetResolution( Display *pdpy, XPContext pcontext, long *dpi );
const char *XpuEmumerateXpAttributeValue( const char *value, void **vcptr );
void XpuDisposeEmumerateXpAttributeValue( void **vc );
Bool XpuParseMediumSourceSize( const char *value, 
                               const char **media_name, Bool *mbool, 
                               float *ma1, float *ma2, float *ma3, float *ma4 );
XPPrinterList XpuGetPrinterList( const char *printer, int *res_list_count );
void XpuFreePrinterList( XPPrinterList list );


#define XpuGetJobAttributes( pdpy, pcontext )     XpGetAttributes( (pdpy), (pcontext), XPJobAttr )
#define XpuGetDocAttributes( pdpy, pcontext )     XpGetAttributes( (pdpy), (pcontext), XPDocAttr )
#define XpuGetPageAttributes( pdpy, pcontext )    XpGetAttributes( (pdpy), (pcontext), XPPageAttr )
#define XpuGetPrinterAttributes( pdpy, pcontext ) XpGetAttributes( (pdpy), (pcontext), XPPrinterAttr )
#define XpuGetServerAttributes( pdpy, pcontext )  XpGetAttributes( (pdpy), (pcontext), XPServerAttr )

void *XpuPrintToFile( Display *pdpy, XPContext pcontext, const char *filename );
XPGetDocStatus XpuWaitForPrintFileChild( void *handle );

_XFUNCPROTOEND

#endif /* !XPRINTUTIL_H */
/* EOF. */
