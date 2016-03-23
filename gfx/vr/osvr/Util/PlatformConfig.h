/** @file
    @brief Auto-configured header

    If this filename ends in `.h`, don't edit it: your edits will
    be lost next time this file is regenerated!

    Must be c-safe!

    @date 2014

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_PlatformConfig_h_GUID_0D10E644_8114_4294_A839_699F39E1F0E0
#define INCLUDED_PlatformConfig_h_GUID_0D10E644_8114_4294_A839_699F39E1F0E0

/** @def OSVR_HAVE_STRUCT_TIMEVAL_IN_WINSOCK2_H
    @brief Does the system have struct timeval in <winsock2.h>?
*/
#define OSVR_HAVE_STRUCT_TIMEVAL_IN_WINSOCK2_H

/** @def OSVR_HAVE_STRUCT_TIMEVAL_IN_SYS_TIME_H
    @brief Does the system have struct timeval in <sys/time.h>?
*/

/*
    MinGW and similar environments have both winsock and sys/time.h, so
    we hide this define for disambiguation at the top level.
*/
#ifndef OSVR_HAVE_STRUCT_TIMEVAL_IN_WINSOCK2_H
/* #undef OSVR_HAVE_STRUCT_TIMEVAL_IN_SYS_TIME_H */
#endif

#if defined(OSVR_HAVE_STRUCT_TIMEVAL_IN_SYS_TIME_H) ||                         \
    defined(OSVR_HAVE_STRUCT_TIMEVAL_IN_WINSOCK2_H)
#define OSVR_HAVE_STRUCT_TIMEVAL
#endif

/**
 * Platform-specific variables.
 *
 * Prefer testing for specific compiler or platform features instead of relying
 * on these variables.
 *
 */
//@{
/* #undef OSVR_AIX */
/* #undef OSVR_ANDROID */
/* #undef OSVR_BSDOS */
/* #undef OSVR_FREEBSD */
/* #undef OSVR_HPUX */
/* #undef OSVR_IRIX */
/* #undef OSVR_LINUX */
/* #undef OSVR_KFREEBSD */
/* #undef OSVR_NETBSD */
/* #undef OSVR_OPENBSD */
/* #undef OSVR_OFS1 */
/* #undef OSVR_SCO_SV */
/* #undef OSVR_UNIXWARE */
/* #undef OSVR_XENIX */
/* #undef OSVR_SUNOS */
/* #undef OSVR_TRU64 */
/* #undef OSVR_ULTRIX */
/* #undef OSVR_CYGWIN */
/* #undef OSVR_MACOSX */
#define OSVR_WINDOWS
//@}

#endif

