/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
    nsCoreGraphicsGlue.cpp

    Glue routines for Core Graphics on Mac OS X.
    
    by Patrick C. Beard.
 */

#include <size_t.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#include <MacErrors.h>
#include <Gestalt.h>
#include <Fonts.h>

#include <CGAffineTransform.h>

static CFBundleRef getBundle(CFStringRef frameworkPath)
{
	CFBundleRef bundle = NULL;
    
	//	Make a CFURLRef from the CFString representation of the bundle's path.
	//	See the Core Foundation URL Services chapter for details.
	CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, frameworkPath, kCFURLPOSIXPathStyle, true);
	if (bundleURL != NULL) {
        bundle = CFBundleCreate(NULL, bundleURL);
        if (bundle != NULL)
        	CFBundleLoadExecutable(bundle);
        CFRelease(bundleURL);
	}
	
	return bundle;
}

static void* getSystemFunction(CFStringRef functionName)
{
  static CFBundleRef systemBundle = getBundle(CFSTR("/System/Library/Frameworks/System.framework"));
  if (systemBundle) return CFBundleGetFunctionPointerForName(systemBundle, functionName);
  return NULL;
}

static void* getQuartzFunction(CFStringRef functionName)
{
  static CFBundleRef quartzBundle = getBundle(CFSTR("/System/Library/Frameworks/ApplicationServices.framework"));
  if (quartzBundle) return CFBundleGetFunctionPointerForName(quartzBundle, functionName);
  return NULL;
}

// Useful Carbon-CFM debugging tool, printf that goes to the system console.

typedef int (*vprintf_proc_ptr) (const char* format, va_list args);
static vprintf_proc_ptr system_vprintf = (vprintf_proc_ptr) getSystemFunction(CFSTR("vprintf"));

int std::printf(const char* format, ...)
{
	int rv = 0;
	va_list args;
	va_start(args, format);
	if (system_vprintf)
		rv = system_vprintf(format, args);
	va_end(args);
	return rv;
}

// Dynamically loaded functions exported from ApplicationServices.framework.

#define quartz_proc_ptr(name) \
    Quartz_ ## name ## _proc_ptr

#define quartz_func(name) \
    Quartz_ ## name
    
#define decl_quartz_func(r_type, name, args) \
    typedef r_type (*quartz_proc_ptr(name)) args; \
    static quartz_proc_ptr(name) quartz_func(name) = (quartz_proc_ptr(name)) getQuartzFunction(CFSTR(#name)); \
    r_type name args

#define call_quartz_func(name, args, err) \
    return (quartz_func(name) ? quartz_func(name) args : err)

#define call_quartz_proc(name, args) \
    if (quartz_func(name)) quartz_func(name) args

decl_quartz_func(OSStatus, QDBeginCGContext, (CGrafPtr inPort, CGContextRef *  outContext))
{
    call_quartz_func(QDBeginCGContext, (inPort, outContext), cfragNoSymbolErr);
}

decl_quartz_func(OSStatus, QDEndCGContext, (CGrafPtr inPort, CGContextRef *  inoutContext))
{
    call_quartz_func(QDEndCGContext, (inPort, inoutContext), cfragNoSymbolErr);
}

decl_quartz_func(OSStatus, ClipCGContextToRegion, (CGContextRef ctx, const Rect * portRect, RgnHandle region))
{
    call_quartz_func(ClipCGContextToRegion, (ctx, portRect, region), cfragNoSymbolErr);
}

decl_quartz_func(void, CGContextScaleCTM, (CGContextRef ctx, float sx, float sy))
{
    call_quartz_proc(CGContextScaleCTM, (ctx, sx, sy));
}

decl_quartz_func(void, CGContextTranslateCTM, (CGContextRef ctx, float tx, float ty))
{
    call_quartz_proc(CGContextTranslateCTM, (ctx, tx, ty));
}

decl_quartz_func(void, CGContextBeginPath, (CGContextRef ctx))
{
    call_quartz_proc(CGContextBeginPath, (ctx));
}

decl_quartz_func(void, CGContextMoveToPoint, (CGContextRef ctx, float x, float y))
{
    call_quartz_proc(CGContextMoveToPoint, (ctx, x, y));
}

decl_quartz_func(void, CGContextAddLineToPoint, (CGContextRef ctx, float x, float y))
{
    call_quartz_proc(CGContextAddLineToPoint, (ctx, x, y));
}

decl_quartz_func(void, CGContextAddLines, (CGContextRef ctx, const CGPoint points[], size_t count))
{
    call_quartz_proc(CGContextAddLines, (ctx, points, count));
}

decl_quartz_func(void, CGContextFillRect, (CGContextRef ctx, CGRect rect))
{
    call_quartz_proc(CGContextFillRect, (ctx, rect));
}

decl_quartz_func(void, CGContextStrokeRect, (CGContextRef ctx, CGRect rect))
{
    call_quartz_proc(CGContextStrokeRect, (ctx, rect));
}

decl_quartz_func(void, CGContextSetRGBFillColor, (CGContextRef ctx, float r, float g, float b, float alpha))
{
    call_quartz_proc(CGContextSetRGBFillColor, (ctx, r, g, b, alpha));
}

decl_quartz_func(void, CGContextSetRGBStrokeColor, (CGContextRef ctx, float r, float g, float b, float alpha))
{
    call_quartz_proc(CGContextSetRGBStrokeColor, (ctx, r, g, b, alpha));
}

decl_quartz_func(void, CGContextSetFont, (CGContextRef ctx, CGFontRef font))
{
    call_quartz_proc(CGContextSetFont, (ctx, font));
}

decl_quartz_func(void, CGContextSetFontSize, (CGContextRef ctx, float size))
{
    call_quartz_proc(CGContextSetFontSize, (ctx, size));
}

decl_quartz_func(void, CGContextSelectFont, (CGContextRef ctx, const char * name, float size, CGTextEncoding textEncoding))
{
    call_quartz_proc(CGContextSelectFont, (ctx, name, size, textEncoding));
}

decl_quartz_func(void, CGContextShowText, (CGContextRef ctx, const char * cstring, size_t length))
{
    call_quartz_proc(CGContextShowText, (ctx, cstring, length));
}

decl_quartz_func(void, CGContextShowTextAtPoint, (CGContextRef ctx, float x, float y, const char * cstring, size_t length))
{
    call_quartz_proc(CGContextShowTextAtPoint, (ctx, x, y, cstring, length));
}

decl_quartz_func(void, CGContextSetTextMatrix, (CGContextRef ctx, CGAffineTransform transform))
{
    call_quartz_proc(CGContextSetTextMatrix, (ctx, transform));
}

decl_quartz_func(CGAffineTransform, CGContextGetTextMatrix, (CGContextRef ctx))
{
    CGAffineTransform transform = { 0.0 };
    call_quartz_func(CGContextGetTextMatrix, (ctx), transform);
}

decl_quartz_func(CGAffineTransform, CGAffineTransformScale, (CGAffineTransform t, float sx, float sy))
{
    call_quartz_func(CGAffineTransformScale, (t, sx, sy), t);
}

decl_quartz_func(CGAffineTransform, CGAffineTransformTranslate, (CGAffineTransform t, float tx, float ty))
{
    call_quartz_func(CGAffineTransformTranslate, (t, tx, ty), t);
}
