/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger Lawrence <rogerl@netscape.com>
 *   Patrick Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <math.h>

#if defined(_WIN32) && !defined(__MWERKS__)
#define __STDC__
#endif

/*
 * Use math routines in fdlibm.
 */

#undef __P
#ifdef __STDC__
#define __P(p)  p
#else
#define __P(p)  ()
#endif

#if defined _WIN32 || defined SUNOS4 

// these are functions we trust the local implementation
// to provide, so we just inline them into calls to the
// standard library.
namespace fd {
    inline double floor(double x)           { return ::floor(x); }
    inline double acos(double x)            { return ::acos(x); }
    inline double asin(double x)            { return ::asin(x); }
    inline double atan(double x)            { return ::atan(x); }
    inline double cos(double x)             { return ::cos(x); }
    inline double sin(double x)             { return ::sin(x); }
    inline double tan(double x)             { return ::tan(x); }
    inline double exp(double x)             { return ::exp(x); }
    inline double log(double x)             { return ::log(x); }
    inline double sqrt(double x)            { return ::sqrt(x); }
    inline double ceil(double x)            { return ::ceil(x); }
    inline double fabs(double x)            { return ::fabs(x); }
    inline double fmod(double x, double y)  { return ::fmod(x, y); }
}

// these one we get from the fdlibm library
namespace fd {
    extern "C" {
        double fd_atan2 __P((double, double));
        double fd_copysign __P((double, double));
        double fd_pow __P((double, double));
    }
    inline double atan2(double x, double y)      { return fd_atan2(x, y); }
    inline double copysign(double x, double y)   { return fd_copysign(x, y); }
    inline double pow(double x, double y)        { return fd_pow(x, y); }
}


#elif defined(linux)

namespace fd {
    inline double atan(double x)               { return ::atan(x); }
    inline double atan2(double x, double y)    { return ::atan2(x, y); }
    inline double ceil(double x)               { return ::ceil(x); }
    inline double cos(double x)                { return ::cos(x); }
    inline double fabs(double x)               { return ::fabs(x); }
    inline double floor(double x)              { return ::floor(x); }
    inline double fmod(double x, double y)     { return ::fmod(x, y); }
    inline double sin(double x)                { return ::sin(x); }
    inline double sqrt(double x)               { return ::sqrt(x); }
    inline double tan(double x)                { return ::tan(x); }
    inline double copysign(double x, double y) { return ::copysign(x, y); }
}

namespace fd {
	extern "C" {
		double fd_asin __P((double));
		double fd_acos __P((double));
		double fd_exp __P((double));
		double fd_log __P((double));
		double fd_pow __P((double, double));
	}
    inline double asin(double x)                 { return fd_asin(x); }
    inline double acos(double x)                 { return fd_acos(x); }
    inline double exp(double x)                  { return fd_exp(x); }
    inline double log(double x)                  { return fd_log(x); }
    inline double pow(double x, double y)        { return fd_pow(x, y); }
}

#elif defined(macintosh) || defined(__APPLE_CC__)

// the macintosh MSL provides acceptable implementations for all of these.
namespace fd {
    inline double atan(double x)               { return ::atan(x); }
    inline double atan2(double x, double y)    { return ::atan2(x, y); }
    inline double ceil(double x)               { return ::ceil(x); }
    inline double cos(double x)                { return ::cos(x); }
    inline double fabs(double x)               { return ::fabs(x); }
    inline double floor(double x)              { return ::floor(x); }
    inline double fmod(double x, double y)     { return ::fmod(x, y); }
    inline double sin(double x)                { return ::sin(x); }
    inline double sqrt(double x)               { return ::sqrt(x); }
    inline double tan(double x)                { return ::tan(x); }
    inline double copysign(double x, double y) { return ::copysign(x, y); }
    inline double asin(double x)               { return ::asin(x); }
    inline double acos(double x)               { return ::acos(x); }
    inline double exp(double x)                { return ::exp(x); }
    inline double log(double x)                { return ::log(x); }
    inline double pow(double x, double y)      { return ::pow(x, y); }
}

#endif

#if defined(_WIN32) && !defined(__MWERKS__)
#undef __STDC__
#endif
