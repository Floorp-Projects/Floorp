/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Macro definitions for various function/variable attributes
 */

#ifndef AVUTIL_ATTRIBUTES_H
#define AVUTIL_ATTRIBUTES_H

#ifdef __GNUC__
#    define AV_GCC_VERSION_AT_LEAST(x,y) (__GNUC__ > x || __GNUC__ == x && __GNUC_MINOR__ >= y)
#else
#    define AV_GCC_VERSION_AT_LEAST(x,y) 0
#endif

#ifndef av_always_inline
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define av_always_inline __attribute__((always_inline)) inline
#else
#    define av_always_inline inline
#endif
#endif

#ifndef av_noinline
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define av_noinline __attribute__((noinline))
#else
#    define av_noinline
#endif
#endif

#ifndef av_pure
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define av_pure __attribute__((pure))
#else
#    define av_pure
#endif
#endif

#ifndef av_const
#if AV_GCC_VERSION_AT_LEAST(2,6)
#    define av_const __attribute__((const))
#else
#    define av_const
#endif
#endif

#ifndef av_cold
#if AV_GCC_VERSION_AT_LEAST(4,3)
#    define av_cold __attribute__((cold))
#else
#    define av_cold
#endif
#endif

#ifndef av_flatten
#if AV_GCC_VERSION_AT_LEAST(4,1)
#    define av_flatten __attribute__((flatten))
#else
#    define av_flatten
#endif
#endif

#ifndef attribute_deprecated
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define attribute_deprecated __attribute__((deprecated))
#else
#    define attribute_deprecated
#endif
#endif

#ifndef av_unused
#if defined(__GNUC__)
#    define av_unused __attribute__((unused))
#else
#    define av_unused
#endif
#endif

/**
 * Mark a variable as used and prevent the compiler from optimizing it
 * away.  This is useful for variables accessed only from inline
 * assembler without the compiler being aware.
 */
#ifndef av_used
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define av_used __attribute__((used))
#else
#    define av_used
#endif
#endif

#ifndef av_alias
#if AV_GCC_VERSION_AT_LEAST(3,3)
#   define av_alias __attribute__((may_alias))
#else
#   define av_alias
#endif
#endif

#ifndef av_uninit
#if defined(__GNUC__) && !defined(__ICC)
#    define av_uninit(x) x=x
#else
#    define av_uninit(x) x
#endif
#endif

#ifdef __GNUC__
#    define av_builtin_constant_p __builtin_constant_p
#    define av_printf_format(fmtpos, attrpos) __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
#    define av_builtin_constant_p(x) 0
#    define av_printf_format(fmtpos, attrpos)
#endif

#endif /* AVUTIL_ATTRIBUTES_H */
