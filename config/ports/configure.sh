#!/bin/sh
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

ARCH="$1"
RELEASE="$2"
MKFILE="$3"
BSDECHO=""
LIB_DIRS=""
MACROS=""
PATH="/usr/ccs/bin:/tools/ns/bin:/tools/contrib/bin:/usr/local/bin:/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/usr/bin/X11:/usr/etc:/usr/ucb:/usr/lpp/xlC/bin"
POSSIBLE_X_DIRS="/usr/X/lib /usr/X11R6/lib /usr/abiccs/lib/X11R5 /usr/X11/lib /usr/lib/X11 /usr/lib /lib"
SPECIAL=""
TMPFILE="foo.$$.c"
X_DIR=""
XFE_MKFILE="xfe_mfile.add"

export PATH

rm -f foo ${TMPFILE}

echo "Let's see what we've got to work with...."

if test "`echo -n blah`" != "-n blah"
then
	BSDECHO="echo"
	ECHO_FLAG="-n"
	ENDER=""
else
	ECHO_FLAG=""
	ENDER="\c"
fi

echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { }" >> ${TMPFILE}
cc -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -eq 0
then
	CC="cc"
else
	CC="gcc"
fi
echo "Compiler seems to be ${CC}...."

echo "Looking for various header files...."

echo ${ECHO_FLAG} "Do we have <stddef.h>....${ENDER}"
if test -f /usr/include/stddef.h
then
	MACROS="${MACROS} -DHAVE_STDDEF_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <stdlib.h>....${ENDER}"
if test -f /usr/include/stdlib.h
then
	MACROS="${MACROS} -DHAVE_STDLIB_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <model.h>....${ENDER}"
if test -f /usr/include/model.h
then
	MACROS="${MACROS} -DHAVE_MODEL_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <bitypes.h>....${ENDER}"
if test -f /usr/include/bitypes.h
then
	MACROS="${MACROS} -DHAVE_BITYPES_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <sys/bitypes.h>....${ENDER}"
if test -f /usr/include/sys/bitypes.h
then
	MACROS="${MACROS} -DHAVE_SYS_BITYPES_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <sys/filio.h>....${ENDER}"
if test -f /usr/include/sys/filio.h
then
	MACROS="${MACROS} -DHAVE_FILIO_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <sys/endian.h>....${ENDER}"
if test -f /usr/include/sys/endian.h
then
	MACROS="${MACROS} -DSYS_ENDIAN_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <ntohl/endian.h> and/or <machine/endian.h>....${ENDER}"
if test -f /usr/include/ntohl/endian.h -o -f /usr/include/machine/endian.h
then
	MACROS="${MACROS} -DNTOHL_ENDIAN_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <machine/endian.h>....${ENDER}"
if test -f /usr/include/machine/endian.h
then
	MACROS="${MACROS} -DMACHINE_ENDIAN_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <sys/machine.h>....${ENDER}"
if test -f /usr/include/sys/machine.h
then
	MACROS="${MACROS} -DSYS_MACHINE_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have <sys/byteorder.h>....${ENDER}"
if test -f /usr/include/sys/byteorder.h
then
	MACROS="${MACROS} -DSYS_BYTEORDER_H"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have either <cdefs.h> or <sys/cdefs.h>....${ENDER}"
if test ! -f /usr/include/cdefs.h -a ! -f /usr/include/sys/cdefs.h
then
	MACROS="${MACROS} -DNO_CDEFS_H"
	echo " no"
else
	echo " yes"
fi

#
# Obviously I have no idea what I _should_ be looking for here....  --briano.
#
echo "Trying to determine if this system is strict SYSV...."
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
if test -f /usr/include/sys/param.h
then
	echo "#include <sys/param.h>" >> ${TMPFILE}
fi
echo "int main() {" >> ${TMPFILE}
echo "#if defined(SVR4) || defined(SYSV) || defined(__svr4) || defined(__svr4__) || defined(_SVR4) || defined(__SVR4) || defined(M_SYSV) || defined(_M_SYSV)" >> ${TMPFILE}
echo "	return(0);" >> ${TMPFILE}
echo "#endif" >> ${TMPFILE}
echo "#if defined(BSD) || defined(bsd) || defined(__bsd) || defined(__bsd__) || defined(_BSD) || defined(__BSD)" >> ${TMPFILE}
echo "	return(1);" >> ${TMPFILE}
echo "#endif" >> ${TMPFILE}
echo "	return(2);" >> ${TMPFILE}
echo "}" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
./foo
case $? in
	0)	echo " -- Looks like SYSVR4...."
		MACROS="${MACROS} -DSYSV -DSVR4 -D__svr4 -D__svr4__"
		;;
	1)	echo " -- Looks like BSD...."
		;;
	2)	echo " -- Can't tell.  Could be either SYSV or BSD, or both...."
		;;

esac

echo "Looking for various platform-specific quirks...."

echo ${ECHO_FLAG} "Do we have long long....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { long long x; }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_LONG_LONG"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have signed long....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { signed long x; }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_SIGNED"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have strerror()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { (void *)strerror(666); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -eq 0
then
	SPECIAL="-DHAVE_STRERROR"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have lchown()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { (void)lchown(\"/foo\",666,666); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -eq 0
then
	MACROS="${MACROS} -DHAVE_LCHOWN"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have memmove()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { char s[32]; const char *s2 = \"diediedie\"; memmove(s,s2,32); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_MEMMOVE"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have alloca()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
if test -f /usr/include/alloca.h
then
	echo "#include <alloca.h>" > ${TMPFILE}
fi
echo "void main() { alloca(666); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_ALLOCA"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have regex()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { (void *)regex(\"foo\",(char *)0); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} -lgen >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_REGEX"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have regcmp()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { (void *)regcmp(\"foo\",\"barfoofrab\"); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} -lgen >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_REGCOMP"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have pgno_t....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { pgno_t foo = 0; }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DHAS_PGNO_T"
	echo " yes"
else
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have IN_MULTICAST....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
if test -f /usr/include/types.h
then
	echo "#include <types.h>" >> ${TMPFILE}
fi
if test -f /usr/include/sys/types.h
then
	echo "#include <sys/types.h>" >> ${TMPFILE}
fi
if test -f /usr/include/netinet/in.h
then
	echo "#include <netinet/in.h>" >> ${TMPFILE}
fi
if test -f /usr/include/linux/in.h
then
	echo "#include <linux/in.h>" >> ${TMPFILE}
fi
if test -f /usr/include/sys/socket.h
then
	echo "#include <sys/socket.h>" >> ${TMPFILE}
fi
echo "void main() {" >> ${TMPFILE}
echo "#ifndef IN_MULTICAST" >> ${TMPFILE}
echo "ERROR" >> ${TMPFILE}
echo "#endif" >> ${TMPFILE}
echo "}" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_MULTICAST"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have tzname[]....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
if test -f /usr/include/time.h
then
	echo "#include <time.h>" >> ${TMPFILE}
fi
if test -f /usr/include/sys/time.h
then
	echo "#include <sys/time.h>" >> ${TMPFILE}
fi
echo "void main() { int foo = tzname[0]; }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNO_TZNAME"
	echo " no"
else
	echo " yes"
fi

echo ${ECHO_FLAG} "Do we have realpath()....${ENDER}"
rm -f foo ${TMPFILE}
echo "#include <stdio.h>" > ${TMPFILE}
echo "void main() { const char *s1 = \"/foo/bar\"; char *s2; (void *)realpath(s1,s2); }" >> ${TMPFILE}
${CC} -o foo ${TMPFILE} >/dev/null 2>&1
if test $? -ne 0
then
	MACROS="${MACROS} -DNEED_REALPATH"
	echo " no"
else
	echo " yes"
fi

rm -f foo ${TMPFILE}

echo ${ECHO_FLAG} "Do we have ranlib....${ENDER}"
ranlib >/dev/null 2>&1
if test $? -eq 0
then
	RANLIB="ranlib"
	echo " yes"
else
	RANLIB="true"
	echo " no"
fi

echo ${ECHO_FLAG} "Do we have xemacs....${ENDER}"
xemacs --version >/dev/null 2>&1
if test $? -eq 0
then
	EMACS="xemacs"
	echo " yes"
else
	EMACS="true"
	echo " no"
fi

echo "Generating ${MKFILE} file...."

if test -x /bin/echo -a -z "${BSDECHO}"
then
	if test "`/bin/echo -n blah`" != "-n blah"
	then
		BSDECHO="/bin/echo"
	fi
fi
if test -x /usr/ucb/echo -a -z "${BSDECHO}"
then
	if test "`/usr/ucb/echo -n blah`" != "-n blah"
	then
		BSDECHO="/usr/ucb/echo"
	fi
fi
if test -z "${BSDECHO}"
then
	BSDECHO="\$(DIST)/bin/bsdecho"
fi

CPU_ARCH="`uname -m`"
case ${CPU_ARCH} in
	000*)
		CPU_ARCH="rs6000"
		;;
	9000*)
		CPU_ARCH="hppa"
		;;
	IP* | RM[45]00 | R[45]000)
		CPU_ARCH="mips"
		;;
	34* | i[3456]86* | i86pc)
		CPU_ARCH="x86"
		;;
	sun4*)
		CPU_ARCH="sparc"
		;;
esac

for i in ${POSSIBLE_X_DIRS}
do
	if test -d ${i}
	then
		LIB_DIRS="${LIB_DIRS} ${i}"
	fi
done
if test ! -z "${LIB_DIRS}"
then
	for i in ${LIB_DIRS}
	do
		if test -f ${i}/libX11.a -o -f ${i}/libX11.so
		then
			X_DIR="${i}"
			break
		fi
	done
fi

rm -f ${MKFILE}
echo "######################################################################" > ${MKFILE}
echo "# Config stuff for ${ARCH}." >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "# Version-independent" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "ARCH			:= `echo ${ARCH} | tr '[A-Z]' '[a-z]'`" >> ${MKFILE}
echo "CPU_ARCH		:= ${CPU_ARCH}" >> ${MKFILE}
echo "GFX_ARCH		:= x" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "CC			= ${CC}" >> ${MKFILE}
echo "CCC			=" >> ${MKFILE}
echo "BSDECHO			= ${BSDECHO}" >> ${MKFILE}
echo "EMACS			= ${EMACS}" >> ${MKFILE}
echo "RANLIB			= ${RANLIB}" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "OS_INCLUDES		= -I/usr/X11/include" >> ${MKFILE}
echo "G++INCLUDES		=" >> ${MKFILE}
echo "LOC_LIB_DIR		= ${X_DIR}" >> ${MKFILE}
echo "MOTIF			=" >> ${MKFILE}
echo "MOTIFLIB		= -lXm" >> ${MKFILE}
echo "OS_LIBS			=" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "PLATFORM_FLAGS		= -D`echo ${ARCH} | tr '[a-z]' '[A-Z]'`" >> ${MKFILE}
echo "MOVEMAIL_FLAGS		= ${SPECIAL}" >> ${MKFILE}
echo "PORT_FLAGS		= -DSW_THREADS ${MACROS}" >> ${MKFILE}
echo "PDJAVA_FLAGS		=" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "OS_CFLAGS		= \$(PLATFORM_FLAGS) \$(PORT_FLAGS) \$(MOVEMAIL_FLAGS)" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "# Version-specific stuff" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "# Overrides for defaults in config.mk (or wherever)" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "# Other" >> ${MKFILE}
echo "######################################################################" >> ${MKFILE}
echo "" >> ${MKFILE}
echo "ifdef SERVER_BUILD" >> ${MKFILE}
echo "STATIC_JAVA		= yes" >> ${MKFILE}
echo "endif" >> ${MKFILE}

echo "Generating a default macro file (${XFE_MKFILE}) to be added to cmd/xfe/Makefile...."
rm -f ${XFE_MKFILE}
echo "" > ${XFE_MKFILE}
echo "########################################" >> ${XFE_MKFILE}
echo "# ${ARCH} ${RELEASE} ${CPU_ARCH}" >> ${XFE_MKFILE}
echo "ifeq (\$(OS_ARCH)\$(OS_RELEASE),${ARCH}${RELEASE})" >> ${XFE_MKFILE}
echo "OTHER_LIBS	= -L${X_DIR} \$(MOTIFLIB) -lXt -lXmu -lXext -lX11 -lm \$(OS_LIBS)" >> ${XFE_MKFILE}
echo "endif" >> ${XFE_MKFILE}
echo "" >> ${XFE_MKFILE}

exit 0
