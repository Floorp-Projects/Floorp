/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

#ifndef __njcpudetect__
#define __njcpudetect__

/***
 * Note: this file should not include *any* other files, nor should it wrap
 * itself in ifdef FEATURE_NANOJIT, nor should it do anything other than
 * define preprocessor symbols.
 */

/***
 * NJ_COMPILER_ARM_ARCH attempts to specify the minimum ARM architecture
 * that the C++ compiler has specified. Note that although Config::arm_arch
 * is initialized to this value by default, there is no requirement that they
 * be in sync.
 *
 * Note, this is done via #define so that downstream preprocessor usage can
 * examine it, but please don't attempt to redefine it.
 *
 * Note, this is deliberately not encased in "ifdef NANOJIT_ARM", as this file
 * may be included before that is defined. On non-ARM platforms we will hit the
 * "Unable to determine" case.
 */

// GCC and RealView usually define __ARM_ARCH__
#if defined(__ARM_ARCH__)

    #define NJ_COMPILER_ARM_ARCH __ARM_ARCH__

// ok, try well-known GCC flags ( see http://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html )
#elif     defined(__ARM_ARCH_7__) || \
        defined(__ARM_ARCH_7A__) || \
        defined(__ARM_ARCH_7M__) || \
        defined(__ARM_ARCH_7R__) || \
        defined(_ARM_ARCH_7)

    #define NJ_COMPILER_ARM_ARCH 7

#elif   defined(__ARM_ARCH_6__) || \
        defined(__ARM_ARCH_6J__) || \
        defined(__ARM_ARCH_6T2__) || \
        defined(__ARM_ARCH_6Z__) || \
        defined(__ARM_ARCH_6ZK__) || \
        defined(__ARM_ARCH_6M__) || \
        defined(_ARM_ARCH_6)

    #define NJ_COMPILER_ARM_ARCH 6

#elif   defined(__ARM_ARCH_5__) || \
        defined(__ARM_ARCH_5T__) || \
        defined(__ARM_ARCH_5E__) || \
        defined(__ARM_ARCH_5TE__)

    #define NJ_COMPILER_ARM_ARCH 5

// Visual C has its own mojo
#elif defined(_MSC_VER) && defined(_M_ARM)

    #define NJ_COMPILER_ARM_ARCH _M_ARM

// RVCT
#elif defined(__TARGET_ARCH_ARM)

    #define NJ_COMPILER_ARM_ARCH __TARGET_ARCH_ARM

#else

    // non-numeric value
    #define NJ_COMPILER_ARM_ARCH "Unable to determine valid NJ_COMPILER_ARM_ARCH (nanojit only supports ARMv5 or later)"

#endif

#endif // __njcpudetect__
