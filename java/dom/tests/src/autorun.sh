#!/bin/sh -x
#
# Created By:     Raju Pallath
# Creation Date:  Aug 2nd 1999
#
# This script is used to  invoke all test case for DOM API
#  thru' apprunner and to recored their results
#
#


# time in seconds after which the apprunner has to be killed.
# by default the apprunner will be up for so much time regardless of
# whether over or not. User can either decrease it or increase it.
#
DELAY_FACTOR=50
SERVER=`hostname`
USER=`logname`
DOCROOT="http://$SERVER/~$USER"


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
   echo "NOTE: You need to copy files test.html and test.xml to "
   echo "      DOCUMENT_ROOT dir. of your Web-Server on this machine."
   echo
   echo
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
# check Document Root and check if files test.html and test.xml
# exists in thos directories
#
##################################################################
#checkDocRoot()
#{
#  echo
#echo "You need to copy files test.html and test.xml to your DOCUMENT_ROOT"
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
#echo "Checking if the files test.html and test.xml exists in DOCUMENT_ROOT..."
#if [ ! -f "$docroot/test.html" ]
#then
#  echo "Could not find 'test.html' in DOCUMENT_ROOT directory"
#  echo "Please copy test.html to DOCUMENT_ROOT and rerun this script"
#  echo
#  exit 1
#fi
#
#if [ ! -f "$docroot/test.xml" ]
#then
#   echo "Could not find 'test.xml' in DOCUMENT_ROOT directory"
#  echo "Please copy test.xml to DOCUMENT_ROOT and rerun this script"
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

   for i in `cat $LOGTXT`
   do
     
     class=`echo $i | cut -d"=" -f1`
     status=`echo $i | cut -d"=" -f2`
     echo "<tr><td>$class</td><td>$status</td></tr>\n" >> $LOGHTML
   done

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
# check Document Root and check if files test.html and test.xml
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
LOGFILE="$curdir/log/BWTestRun.log"
LOGTXT="$curdir/log/BWTest.txt"
LOGHTML="$curdir/log/BWTest.html"


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

if [ !  -x "$mozhome/apprunner"  ]
then
  echo "Could not find executable 'apprunner' in MOZILLA_FIVE_HOME."
  echo "Please check your setting..."
  echo
  exit 1;
fi


chkpid=`ps -aef | grep apprunner | grep -v grep | awk '{ print $2 }'` 
if [ ! -z "$chkpid" ]
then
   echo "Detected an instance of apprunner...";
   echo "Exit out from apprunner and restart this script..."
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
DOCFILE="$DOCROOT/test.html"
runcnt=1
filename="$curdir/BWTestClass.lst.ORIG"

if [ "$runtype" = "1" ]
then
  DOCFILE="$DOCROOT/test.html"
  runcnt=1
  filename="$curdir/BWTestClass.lst.html.ORIG"
fi

if [ "$runtype" = "2" ]
then
  DOCFILE="$DOCROOT/test.xml"
  filename="$curdir/BWTestClass.lst.xml.ORIG"
  runcnt=1
fi

if [ "$runtype" = "3" ]
then
  DOCFILE="$DOCROOT/test.html"
  filename="$curdir/BWTestClass.lst.html.ORIG"
  runcnt=2
fi


echo "Runtype is $runtype"
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

   echo $testcase > $curdir/BWTestClass.lst
   cd $MOZILLA_FIVE_HOME
   
   echo "========================================"
   logstr="Running TestCase $testcase...."
   echo $logstr
   constructLogString $logstr

   format=`echo $testcase | sed 's/\./\//g'`
   nom=`basename $format`
   testlog="$curdir/log/$nom.$id.log"
   ./apprunner $DOCFILE 2>$testlog 1>&2 &
   curpid=`ps -aef | grep apprunner | grep -v grep | awk '{ print $2 }'`

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

      chkpid=`ps -aef | grep apprunner | grep -v grep | awk '{ print $2 }'` 
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
  
   #if [ $flag -eq 0 ]
   #then
   #   /bin/rm $testlog
   #fi
 done

 currcnt=`expr $currcnt + 1`

 if [ $currcnt -eq $runcnt ]
 then
      break
 fi

 if [ "$runtype" = "3" ]
 then
     DOCFILE="$DOCROOT/test.xml"
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
