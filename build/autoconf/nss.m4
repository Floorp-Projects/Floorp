# -*- tab-width: 4; -*-
# Configure paths for NSS
# Public domain - Chris Seawood <cls@seawood.org> 2001-04-05
# Based upon gtk.m4 (also PD) by Owen Taylor

dnl AM_PATH_NSS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for NSS, and define NSS_CFLAGS and NSS_LIBS
AC_DEFUN([AM_PATH_NSS],
[dnl

AC_ARG_WITH(nss-prefix,
	[  --with-nss-prefix=PFX   Prefix where NSS is installed],
	nss_config_prefix="$withval",
	nss_config_prefix="")

AC_ARG_WITH(nss-exec-prefix,
	[  --with-nss-exec-prefix=PFX
                          Exec prefix where NSS is installed],
	nss_config_exec_prefix="$withval",
	nss_config_exec_prefix="")

	if test -n "$nss_config_exec_prefix"; then
		nss_config_args="$nss_config_args --exec-prefix=$nss_config_exec_prefix"
		if test -z "$NSS_CONFIG"; then
			NSS_CONFIG=$nss_config_exec_prefix/bin/nss-config
		fi
	fi
	if test -n "$nss_config_prefix"; then
		nss_config_args="$nss_config_args --prefix=$nss_config_prefix"
		if test -z "$NSS_CONFIG"; then
			NSS_CONFIG=$nss_config_prefix/bin/nss-config
		fi
	fi

	unset ac_cv_path_NSS_CONFIG
	AC_PATH_PROG(NSS_CONFIG, nss-config, no)
	min_nss_version=ifelse([$1], ,3.0.0,$1)
	AC_MSG_CHECKING(for NSS - version >= $min_nss_version)

	no_nss=""
	if test "$NSS_CONFIG" = "no"; then
		no_nss="yes"
	else
		NSS_CFLAGS=`$NSS_CONFIG $nss_config_args --cflags`
		NSS_LIBS=`$NSS_CONFIG $nss_config_args --libs`

		nss_config_major_version=`$NSS_CONFIG $nss_config_args --version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\(\.\([[0-9]]*\)\)\{0,1\}/\1/'`
		nss_config_minor_version=`$NSS_CONFIG $nss_config_args --version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\(\.\([[0-9]]*\)\)\{0,1\}/\2/'`
		nss_config_micro_version=`$NSS_CONFIG $nss_config_args --version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\(\.\([[0-9]]*\)\)\{0,1\}/\4/'`
		if test -z "$nss_config_micro_version"; then
			nss_config_micro_version="0"
		fi

		min_nss_major_version=`echo $min_nss_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\(\.\([[0-9]]*\)\)\{0,1\}/\1/'`
		min_nss_minor_version=`echo $min_nss_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\(\.\([[0-9]]*\)\)\{0,1\}/\2/'`
		min_nss_micro_version=`echo $min_nss_version | \
			sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\(\.\([[0-9]]*\)\)\{0,1\}/\4/'`
		if test -z "$min_nss_micro_version"; then
			min_nss_micro_version="0"
		fi

		if test "$nss_config_major_version" -lt "$min_nss_major_version"; then
			no_nss="yes"
		elif test "$nss_config_major_version" -eq "$min_nss_major_version" &&
		     test "$nss_config_minor_version" -lt "$min_nss_minor_version"; then
			no_nss="yes"
		elif test "$nss_config_major_version" -eq "$min_nss_major_version" &&
		     test "$nss_config_minor_version" -eq "$min_nss_minor_version" &&
		     test "$nss_config_micro_version" -lt "$min_nss_micro_version"; then
			no_nss="yes"
		fi
	fi

	if test -z "$no_nss"; then
		AC_MSG_RESULT(yes)
		ifelse([$2], , :, [$2])     
	else
		AC_MSG_RESULT(no)
		ifelse([$3], , :, [$3])
	fi


	AC_SUBST_LIST(NSS_CFLAGS)
	AC_SUBST_LIST(NSS_LIBS)

])
