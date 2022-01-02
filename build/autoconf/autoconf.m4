dnl Driver that loads the Autoconf macro files.
dnl Requires GNU m4.
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
dnl Written by David MacKenzie.
dnl
include(acgeneral.m4)dnl
builtin(include, acspecific.m4)dnl
builtin(include, acoldnames.m4)dnl
dnl Do not sinclude acsite.m4 here, because it may not be installed
dnl yet when Autoconf is frozen.
dnl Do not sinclude ./aclocal.m4 here, to prevent it from being frozen.
