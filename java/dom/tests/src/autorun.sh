#!/bin/sh -x
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s):

##################################################################
# This script is used to  invoke all test case for DOM API
#  thru' mozilla-bin and to record their results
#
##################################################################
#
# time in seconds after which the mozilla-bin has to be killed.
# by default the mozilla-bin will be up for so much time regardless of
# whether over or not. User can either decrease it or increase it.
#
DELAY_FACTOR=50
SERVER=`hostname`
USER=`logname`
#DOCROOT="http://$SERVER/~$USER"
curdir=`pwd`
DOCROOT="file://$curdir"
ADDITIONAL_PARAMETERS="-P default"



##################################################################
# Usage
##################################################################
usage()
{

   echo
   echo "##################################################################"
   echo "   sh autorun.sh [ -t <test case> ]"
   echo
   echo " where <test case> is one of the test cases prefixed with package"
   echo " name ex: org.mozilla.dom.AttrImpl_getSpecified"
   echo
   echo "##################################################################"
   echo
}

##################################################################
# Title display
##################################################################
title()
{

   echo
   echo "################################################"
   echo "   Automated Execution of DOM API TestSuite"
   echo "################################################"
   echo
   echo "NOTE: You need to copy files test.html and"
   echo "      testxml.html to DOCUMENT_ROOT dir. of"
   echo "      your Web-Server on this machine."
   echo
   echo
}

##################################################################
#
# check whether to run tests in single threaded or multi threaded
# mode
#
##################################################################
checkExecutionMode()
{
	executionMode=""
        propnom="$curdir/BWProperties" 
        executionMode=`grep THREADMODE $propnom | cut -d"=" -f2`

        if [ "$executionMode" != "S" ] &&
	   [ "$executionMode" != "s" ] &&
	   [ "$executionMode" != "M" ] &&
	   [ "$executionMode" != "m" ] 
        then
          echo "Entry for BW_THREADMODE in BWProperties file is incorrect."
          echo "It should be set to 'S' or 'M'"
          echo
          echo " Make changes to BWProperties file and re-run this script"
          echo
          exit 1
        fi

}


##################################################################
#
# check which tests to run. XML/HTML or both
#
##################################################################
checkRun()
{
	runtype="0"
	while true
	do
	  echo
	  echo "Run 1) HTML Test suite."
	  echo "    2) XML Test suite."
	  echo "    3) BOTH HTML and XML."
	  echo
	  echo "Enter choice (1-3) :"; read runtype

	  if [ ! -z "$runtype" ]
	  then
	     if [ "$runtype" != "1" ] &&
	        [ "$runtype" != "2" ] &&
	        [ "$runtype" != "3" ]
	     then
	       echo "Invaid choice. Range is from 1-3..."
	       echo
	       continue   
	     fi
	     break
	  fi
	done


}

##################################################################
#
# check Document Root and check if files test.html and testxml.html
# exists in thos directories
#
##################################################################
#checkDocRoot()
#{
#  echo
#echo "You need to copy files test.html and testxml.html to your DOCUMENT_ROOT"
#echo "of your Web-Server"
#echo
#echo "This test assumes that you have set up you WebServer and copied the"
#echo "files to htdocs directory on your WebServer."
#echo
#docroot="";
#while true
#do
#  echo "Enter your DOCUMENT_ROOT directory of your Web-Server
#  read docroot
#
#  if [ ! -z "$docroot" ]
#  then
#     if [ ! -d "$docroot" ]
#     then
#       echo "$docroot directory does not exist..."
#       echo
#       continue   
#     fi
#     break
#  fi
#done
#
#
#
#echo
#echo "Checking if the files test.html and testxml.html exists in DOCUMENT_ROOT..."
#if [ ! -f "$docroot/test.html" 
#then
#  echo "Could not find 'test.html' in DOCUMENT_ROOT directory"
#  echo "Please copy test.html to DOCUMENT_ROOT and rerun this script"
#  echo
#  exit 1
#fi
#
#if [ ! -f "$docroot/testxml.html" ]
#then
#   echo "Could not find 'testxml.html' in DOCUMENT_ROOT directory"
#  echo "Please copy testxml.html to DOCUMENT_ROOT and rerun this script"
#  echo
#  exit 1
#fi
#}


#########################################################################
#
# Append table entries to Output HTML File 
#
#########################################################################
appendEntries()
{
   echo "<tr><td></td><td></td></tr>\n" >> $LOGHTML
   echo "<tr><td></td><td></td></tr>\n" >> $LOGHTML
   echo "<tr bgcolor=\"#FF6666\"><td>Test Status (XML)</td><td>Result</td></tr>\n" >> $LOGHTML
}
#########################################################################
#
# Construct Output HTML file Header
#
#########################################################################
constructHTMLHeader()
{
   echo "<html><head><title>\n" >> $LOGHTML
   echo "DOMAPI Core Level 1 Test Status\n" >> $LOGHTML
   echo "</title></head><body bgcolor=\"white\">\n" >> $LOGHTML
   echo "<center><h1>\n" >> $LOGHTML
   echo "DOM API Automated TestRun Results\n" >> $LOGHTML
   dt=`date`
   echo  "</h1><h2>$dt" >> $LOGHTML
   echo "</h2></center>\n" >> $LOGHTML
   echo "<hr noshade>" >> $LOGHTML
   echo "<table bgcolor=\"#99FFCC\">\n" >> $LOGHTML
   echo "<tr bgcolor=\"#FF6666\">\n" >> $LOGHTML
   echo "<td>Test Case</td>\n" >> $LOGHTML
   echo "<td>Result</td>\n" >> $LOGHTML
   echo "</tr>\n" >> $LOGHTML
}


#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################
constructHTMLBody()
{
   sort -u $LOGTXT > $LOGTXT.tmp
   /bin/mv $LOGTXT.tmp $LOGTXT

   while read i
   do
     
     class=`echo $i | cut -d"=" -f1`
     status=`echo $i | cut -d"=" -f2`
     echo "<tr><td>$class</td><td>$status</td></tr>\n" >> $LOGHTML
   done < $LOGTXT

}

#########################################################################
#
# Construct Output HTML file Footer
#
#########################################################################
constructHTMLFooter()
{
   echo "</table></body></html>\n" >> $LOGHTML
}

#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################
constructHTML()
{
   constructHTMLHeader
   constructHTMLBody
}

#########################################################################
#
# Construct LogFile Header. The Log file is always appended with entries
#
#########################################################################
constructLogHeader()
{
   echo >> $LOGFILE
   echo >> $LOGFILE
   str="###################################################"
   echo $str >> $LOGFILE
   dt=`date`
   str="Logging Test Run on $dt ..."
   echo $str >> $LOGFILE
   str="###################################################"
   echo $str >> $LOGFILE
   echo >> $LOGFILE

   echo "All Log Entries are maintained in LogFile $LOGFILE"
   echo
}

#########################################################################
#
# Construct LogFile Footer. 
#
#########################################################################
constructLogFooter()
{
   echo >> $LOGFILE
   echo $str >> $LOGFILE
   dt=`date`
   str="End of Logging Test $dt ..."
   echo $str >> $LOGFILE
   str="###################################################"
   echo $str >> $LOGFILE
   echo >> $LOGFILE

  
}


##################################################################
#
# check Document Root and check if files test.html and testxml.html
# exists in thos directories
#
##################################################################
constructLogString()
{
   if [ $# -eq 0 ]
   then
        echo "constructLogString <testcase>"
        echo
        return
   fi
   logstring="$*"

   echo "$logstring" >> $LOGFILE
  
}


##################################################################
# main
##################################################################
clear
title

curdir=`pwd`
LOGDIRECTORY="$curdir/log";
LOGFILE="$curdir/log/BWTestRun.log"
LOGTXT="$curdir/log/BWTest.txt"
LOGHTML="$curdir/log/BWTest.html"

/bin/mkdir -p "$LOGDIRECTORY";

testparam="";
if [ $# -gt 2 ] 
then
 usage
 exit 1
fi

if [ $# -gt 1 ]
then
  if [ "$1" != "-t" ]
  then
     usage
     exit 1
  else
     testparam=$2 
  fi
fi

if [ -z "$testparam" ]
then
  echo "WARNING: Your are going to execute the whole test suite...."
fi   

checkRun

mozhome=${MOZILLA_FIVE_HOME}
if [ -z "$mozhome" ]
then
  echo "MOZILLA_FIVE_HOME is not set. Please set it and rerun this script...."
  echo
  exit 1;
fi

if [ !  -x "$mozhome/mozilla-bin"  ]
then
  echo "Could not find executable 'mozilla-bin' in MOZILLA_FIVE_HOME."
  echo "Please check your setting..."
  echo
  exit 1;
fi


chkpid=`ps -aef | grep mozilla-bin | grep -v grep | awk '{ print $2 }'` 
if [ ! -z "$chkpid" ]
then
   echo "Detected an instance of mozilla-bin...";
   echo "Exit out from mozilla-bin and restart this script..."
   exit 1
fi


# Remove core file
if [ -f "$curdir/core" ]
then
   /bin/rm "$curdir/core"
fi

# Backup existing .lst file
if [ -f "$curdir/BWTestClass.lst" ]
then
   /bin/mv "$curdir/BWTestClass.lst" "$curdir/BWTestClass.lst.bak"
fi

# Check if ORIG list file is present
# this file contains all test cases
if [ ! -f "$curdir/BWTestClass.lst.ORIG" ]
then
   echo
   echo "File BWTestClass.lst.ORIG not found ...";
   echo "Check Mozilla Source base and bringover this file....";
   echo
fi

id=$$
# Check if BWProperties file exists
#
if [  -f "$MOZILLA_FIVE_HOME/BWProperties" ]
then
  newfile="$MOZILLA_FIVE_HOME/BWProperties.$id"
  /bin/mv "$MOZILLA_FIVE_HOME/BWProperties" $newfile
fi
/bin/cp "$curdir/BWProperties" "$MOZILLA_FIVE_HOME/BWProperties"

# Read the LOGDIR setting from BWProperties and
# accordingly set BWTest.txt location
#
logdir=`grep BW_LOGDIR $curdir/BWProperties | cut -d"=" -f2`
if [ ! -d "$logdir" ]
then
  echo
  echo "Log Directory $logdir does not exist...";
  echo "Create Directory and re-run this script...";
  echo
  exit 1
fi
LOGTXT="$logdir/BWTest.txt"


# check BW_THREADMODE setting from BWPropeties file
#
checkExecutionMode

# check if output text file of previous run exists.
# if so, then save it as .bak
#
if [ -f "$LOGTXT" ]
then
  newfile=$LOGTXT".bak"
  /bin/mv $LOGTXT $newfile
fi
/bin/touch $LOGTXT

# check if output html file of previous run exists.
# if so, then save it as .bak
#
if [ -f "$LOGHTML" ]
then
  newfile=$LOGHTML".bak"
  /bin/mv $LOGHTML $newfile
fi
/bin/touch $LOGHTML

# construct DOCFILE
appreg=${USE_APPLET_FOR_REGISTRATION}
if [ -z "$appreg" ]
then
	DOCFILE="$DOCROOT/test.html";
else
	DOCFILE="$DOCROOT/TestLoaderHTML.html";
fi

runcnt=1
filename="$curdir/BWTestClass.lst.ORIG"

if [ "$runtype" = "1" ]
then
  if [ -z "$appreg" ]
  then
	DOCFILE="$DOCROOT/test.html";
  else
	DOCFILE="$DOCROOT/TestLoaderHTML.html";
  fi
  runcnt=1
  filename="$curdir/BWTestClass.lst.html.ORIG"
fi

if [ "$runtype" = "2" ]
then
  DOCFILE="$DOCROOT/testxml.html"
  if [ -z "$appreg" ]
  then
	DOCFILE="$DOCROOT/testxml.html";
  else
	DOCFILE="$DOCROOT/TestLoaderXML.html";
  fi
  filename="$curdir/BWTestClass.lst.xml.ORIG"
  runcnt=1
fi

if [ "$runtype" = "3" ]
then
  if [ -z "$appreg" ]
  then
	DOCFILE="$DOCROOT/test.html";
  else
	DOCFILE="$DOCROOT/TestLoaderHTML.html";
  fi
  filename="$curdir/BWTestClass.lst.html.ORIG"
  runcnt=2
fi


constructLogHeader

CLASSPATH="$curdir/../classes:${CLASSPATH}"


currcnt=0
while true
do


 for i in `cat $filename`
 do
   if [ ! -z "$testparam" ]
   then
      testcase=$testparam
   else
     testcase=$i;
   fi

   x=`echo $testcase | grep "#"`
   if [ $? -eq 0 ]
   then
      continue
   fi

   # if single threaded execution 
   if [ "$executionMode" = "S" ]
   then
      echo $testcase > $curdir/BWTestClass.lst
   else
      # if multi-threaded but user has specified particular testcase
      if [ ! -z "$testparam" ]
      then
        echo $testcase > $curdir/BWTestClass.lst
      else
        /bin/cp  $filename $curdir/BWTestClass.lst
      fi
   fi

   cd $MOZILLA_FIVE_HOME
   
   echo "========================================"
   logstr="Running TestCase $testcase...."
   echo $logstr
   constructLogString $logstr

   format=`echo $testcase | sed 's/\./\//g'`
   nom=`basename $format`
   testlog="$curdir/log/$nom.$id.log"
   ./mozilla-bin $ADDITIONAL_PARAMETERS $DOCFILE 2>$testlog 1>&2 &

   # dummy sleep to allow mozilla-bin to show up on process table
   sleep 3
   curpid=`ps -aef | grep mozilla-bin | grep -v grep | awk '{ print $2 }'`

   flag=0
   cnt=0
   while true
   do
      sleep 10

      if [ -f "$MOZILLA_FIVE_HOME/core" ]
      then
         /bin/rm "$MOZILLA_FIVE_HOME/core"
         logstr="Test dumped core..."
         constructLogString "$logstr"
         logstr="Test FAILED..."
         constructLogString "$logstr"
         logstr="Check ErrorLog File $testlog"
         constructLogString "$logstr"
         constructLogString ""
         constructLogString ""

         echo "$testcase=FAILED" >> $LOGTXT
         flag=1
         break
      fi

      chkpid=`ps -aef | grep mozilla-bin | grep -v grep | awk '{ print $2 }'` 
      if [ -z "$chkpid" ]
      then
           flag=1
           break
      fi

      if [ "$curpid" != "$chkpid" ]
      then
           flag=1
           break;
      fi

      cnt=`expr $cnt + 10`
      if [ $cnt -eq $DELAY_FACTOR ]
      then
         flag=0
         #logstr="TestCase Execution PASSED..."
         #constructLogString $logstr
         kill -9 $chkpid 2>/dev/null
         break
      fi

   done
   kill -9 $curpid 2>/dev/null

   if [ ! -z "$testparam" ]
   then
      break
   fi
  
   # if single threaded execution 
   if [ "$executionMode" = "M" ]
   then
        break
   fi
 done

 currcnt=`expr $currcnt + 1`

 if [ $currcnt -eq $runcnt ]
 then
      break
 fi

 if [ "$runtype" = "3" ]
 then
     DOCFILE="$DOCROOT/testxml.html"
     filename="$curdir/BWTestClass.lst.xml.ORIG"
     constructHTML
     appendEntries
     if [ -f "$LOGTXT" ]
     then
       newfile=$LOGTXT".bak"
       /bin/mv $LOGTXT $newfile
     fi
     /bin/touch $LOGTXT
 fi
done

constructLogFooter

if [ "$runtype" = "3" ]
then
 constructHTMLBody
else
 constructHTML
fi
constructHTMLFooter

newfile="$MOZILLA_FIVE_HOME/BWProperties.$id"
/bin/mv "$newfile" "$MOZILLA_FIVE_HOME/BWProperties"
cd $curdir
