dnl Map old names of Autoconf macros to new regularized names.
dnl This file is part of Autoconf.
dnl Copyright (C) 1994 Free Software Foundation, Inc.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
dnl 02111-1307, USA.
dnl
dnl General macros.
dnl
define(AC_WARN, [indir([AC_MSG_WARN], $@)])dnl
define(AC_ERROR, [indir([AC_MSG_ERROR], $@)])dnl
AC_DEFUN(AC_PROGRAM_CHECK, [indir([AC_CHECK_PROG], $@)])dnl
AC_DEFUN(AC_PROGRAM_PATH, [indir([AC_PATH_PROG], $@)])dnl
AC_DEFUN(AC_PROGRAMS_CHECK, [indir([AC_CHECK_PROGS], $@)])dnl
AC_DEFUN(AC_PROGRAMS_PATH, [indir([AC_PATH_PROGS], $@)])dnl
AC_DEFUN(AC_PREFIX, [indir([AC_PREFIX_PROGRAM], $@)])dnl
AC_DEFUN(AC_HEADER_EGREP, [indir([AC_EGREP_HEADER], $@)])dnl
AC_DEFUN(AC_PROGRAM_EGREP, [indir([AC_EGREP_CPP], $@)])dnl
AC_DEFUN(AC_TEST_PROGRAM, [indir([AC_TRY_RUN], $@)])dnl
AC_DEFUN(AC_TEST_CPP, [indir([AC_TRY_CPP], $@)])dnl
AC_DEFUN(AC_HEADER_CHECK, [indir([AC_CHECK_HEADER], $@)])dnl
AC_DEFUN(AC_FUNC_CHECK, [indir([AC_CHECK_FUNC], $@)])dnl
AC_DEFUN(AC_HAVE_FUNCS, [indir([AC_CHECK_FUNCS], $@)])dnl
AC_DEFUN(AC_HAVE_HEADERS, [indir([AC_CHECK_HEADERS], $@)])dnl
AC_DEFUN(AC_SIZEOF_TYPE, [indir([AC_CHECK_SIZEOF], $@)])dnl
dnl
dnl Specific macros.
dnl
AC_DEFUN(AC_GCC_TRADITIONAL, [indir([AC_PROG_GCC_TRADITIONAL])])dnl
AC_DEFUN(AC_MINUS_C_MINUS_O, [indir([AC_PROG_CC_C_O])])dnl
AC_DEFUN(AC_SET_MAKE, [indir([AC_PROG_MAKE_SET])])dnl
AC_DEFUN(AC_YYTEXT_POINTER, [indir([AC_DECL_YYTEXT])])dnl
AC_DEFUN(AC_LN_S, [indir([AC_PROG_LN_S])])dnl
AC_DEFUN(AC_STDC_HEADERS, [indir([AC_HEADER_STDC])])dnl
AC_DEFUN(AC_MAJOR_HEADER, [indir([AC_HEADER_MAJOR])])dnl
AC_DEFUN(AC_STAT_MACROS_BROKEN, [indir([AC_HEADER_STAT])])dnl
AC_DEFUN(AC_SYS_SIGLIST_DECLARED, [indir([AC_DECL_SYS_SIGLIST])])dnl
AC_DEFUN(AC_GETGROUPS_T, [indir([AC_TYPE_GETGROUPS])])dnl
AC_DEFUN(AC_UID_T, [indir([AC_TYPE_UID_T])])dnl
AC_DEFUN(AC_SIZE_T, [indir([AC_TYPE_SIZE_T])])dnl
AC_DEFUN(AC_PID_T, [indir([AC_TYPE_PID_T])])dnl
AC_DEFUN(AC_OFF_T, [indir([AC_TYPE_OFF_T])])dnl
AC_DEFUN(AC_MODE_T, [indir([AC_TYPE_MODE_T])])dnl
AC_DEFUN(AC_RETSIGTYPE, [indir([AC_TYPE_SIGNAL])])dnl
AC_DEFUN(AC_MMAP, [indir([AC_FUNC_MMAP])])dnl
AC_DEFUN(AC_VPRINTF, [indir([AC_FUNC_VPRINTF])])dnl
AC_DEFUN(AC_VFORK, [indir([AC_FUNC_VFORK])])dnl
AC_DEFUN(AC_WAIT3, [indir([AC_FUNC_WAIT3])])dnl
AC_DEFUN(AC_ALLOCA, [indir([AC_FUNC_ALLOCA])])dnl
AC_DEFUN(AC_GETLOADAVG, [indir([AC_FUNC_GETLOADAVG])])dnl
AC_DEFUN(AC_UTIME_NULL, [indir([AC_FUNC_UTIME_NULL])])dnl
AC_DEFUN(AC_STRCOLL, [indir([AC_FUNC_STRCOLL])])dnl
AC_DEFUN(AC_SETVBUF_REVERSED, [indir([AC_FUNC_SETVBUF_REVERSED])])dnl
AC_DEFUN(AC_TIME_WITH_SYS_TIME, [indir([AC_HEADER_TIME])])dnl
AC_DEFUN(AC_TIMEZONE, [indir([AC_STRUCT_TIMEZONE])])dnl
AC_DEFUN(AC_ST_BLOCKS, [indir([AC_STRUCT_ST_BLOCKS])])dnl
AC_DEFUN(AC_ST_BLKSIZE, [indir([AC_STRUCT_ST_BLKSIZE])])dnl
AC_DEFUN(AC_ST_RDEV, [indir([AC_STRUCT_ST_RDEV])])dnl
AC_DEFUN(AC_CROSS_CHECK, [indir([AC_C_CROSS])])dnl
AC_DEFUN(AC_CHAR_UNSIGNED, [indir([AC_C_CHAR_UNSIGNED])])dnl
AC_DEFUN(AC_LONG_DOUBLE, [indir([AC_C_LONG_DOUBLE])])dnl
AC_DEFUN(AC_WORDS_BIGENDIAN, [indir([AC_C_BIGENDIAN])])dnl
AC_DEFUN(AC_INLINE, [indir([AC_C_INLINE])])dnl
AC_DEFUN(AC_CONST, [indir([AC_C_CONST])])dnl
AC_DEFUN(AC_LONG_FILE_NAMES, [indir([AC_SYS_LONG_FILE_NAMES])])dnl
AC_DEFUN(AC_RESTARTABLE_SYSCALLS, [indir([AC_SYS_RESTARTABLE_SYSCALLS])])dnl
AC_DEFUN(AC_FIND_X, [indir([AC_PATH_X])])dnl
AC_DEFUN(AC_FIND_XTRA, [indir([AC_PATH_XTRA])])dnl
