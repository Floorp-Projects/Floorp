/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See 
 * the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * The Original Code is this file as it appears in the Mozilla source tree.
 *
 * The Initial Developer of this code under the MPL is Christopher
 * Seawood, <cls@seawood.org>.  Portions created by Christopher Seawood 
 * are Copyright (C) 1998 Christopher Seawood.  All Rights Reserved.
 */

/* The purpose of this file is to convert various compiler defines into
 * a single define for each platform.  This will allow developers to 
 * continue to use -Dplatform WHERE NECESSARY instead of learning each
 * set of compiler defines. 
 */
#ifndef _platform_h
#define _platform_h

#ifdef __sun
#ifdef __SVR4
#undef SOLARIS
#define SOLARIS 1
#else
#undef SUNOS4
#define SUNOS4 1
#endif /* __SVR4 */
#endif /* __sun */

#ifdef linux
#undef LINUX
#define LINUX 1
#endif /* linux */

#endif /* _platform_h */
