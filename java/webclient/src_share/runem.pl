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
# Ed Burns <edburns@acm.org>
# Macadamian Technologies :
#      Louis-philippe Gagnon
#      Jason Mawdsley


# this script must be run in the directory in which it resides.

#
# Verification, usage checking
#
$ARGC = $#ARGV + 1;
$MIN_ARGC = 2;

if ($MIN_ARGC > $ARGC || !($ENV{"MOZILLA_FIVE_HOME"})) {
  if (!($ENV{"MOZILLA_FIVE_HOME"})) {
     print "MOZILLA_FIVE_HOME must be set to the absolute path\nto the mozilla bin directory.\n";
 }
  print "usage runem.pl <class name> <depth>\n";
  exit -1;
}

#
# Constant definitions
#

$CLASSNAME = $ARGV[0];
$DEPTH = $ARGV[1];

# determine the path separator
$_ = $ENV{PATH};
if (m|/|) {
  $SEP = "/";
  $CPSEP = ":";
}
else {
  $SEP = "\\";
  $CPSEP = ";";
}

if ($SEP eq "/") {
  $IS_UNIX = 1;
}

$BINDIR = $ENV{"MOZILLA_FIVE_HOME"};

$JAVA_CMD = $ENV{"JDKHOME"} . $SEP . "bin" . $SEP . "java";

#
# set up environment vars
#

# prepend mozilla dist to path
$ENV{PATH} = $BINDIR . $CPSEP . $ENV{PATH};

# if on UNIX, stock the LD_LIBRARY_PATH
if ($IS_UNIX) {
  # append the GTK lib dirs
  open(GTK_CONFIG, "gtk-config --libs |");
  $_ = <GTK_CONFIG>;
  close(GTK_CONFIG);
  @libs = split;
  foreach $_ (@libs) {
    if (/-L/) {
      $ENV{"LD_LIBRARY_PATH"} = $ENV{"LD_LIBRARY_PATH"} . ":" . substr($_,2);
    }
  }

  # append the JDK lib dirs
  $ENV{"LD_LIBRARY_PATH"} = $ENV{"LD_LIBRARY_PATH"} . ":" .
    $ENV{"JDKHOME"} . $SEP . "jre" . $SEP . "lib" . $SEP . "sparc";
  $ENV{"LD_LIBRARY_PATH"} = $ENV{"LD_LIBRARY_PATH"} . ":" .
    $ENV{"JDKHOME"} . $SEP . "jre" . $SEP . "lib" . $SEP . "sparc" . $SEP . 
      "native_threads";
  $ENV{"LD_LIBRARY_PATH"} = $ENV{"LD_LIBRARY_PATH"} . ":" .
    $ENV{"JDKHOME"} . $SEP . "jre" . $SEP . "lib" . $SEP . "sparc" . $SEP . 
      "classic";
}

# stock the CLASSPATH
#$ENV{"CLASSPATH"} = $ENV{"JDKHOME"} . $SEP . "lib" . $SEP . "tools.jar" . 
#  $CPSEP . $ENV{"JDKHOME"} . $SEP . "lib" . $SEP . "rt.jar" . $CPSEP . 
#  $ENV{"CLASSPATH"};
if ($IS_UNIX) {
  $ENV{"CLASSPATH"} = $ENV{"CLASSPATH"} . $CPSEP . $BINDIR . $SEP . ".." . 
    $SEP . "classes";
}
else {
  $ENV{"CLASSPATH"} = $ENV{"CLASSPATH"} . $CPSEP . $DEPTH . $SEP . "dist" . 
    $SEP . "classes";
}

# build up the command invocation string

$cmd = $JAVA_CMD;
# if on UNIX, append the -native argument
if ($SEP eq "/") {
  $cmd = $cmd . " -native";
}

#tack on the java library path
$cmd = $cmd . " -Djava.library.path=" . $BINDIR . $CPSEP . $BINDIR . $SEP . "components";
if ($IS_UNIX) {
  $cmd = $cmd . $CPSEP . $ENV{"LD_LIBRARY_PATH"};
}

#tack on the debug arguments
if ($ENV{"MOZ_DEBUG"}) {
  if ($IS_UNIX) {
    $cmd = $cmd . " -Xdebug -Xrunjdwp:transport=dt_socket,address=8000,server=y,suspend=n";
  }
  else {
    $cmd = $cmd . " -Xdebug -Xrunjdwp:transport=dt_shmem,address=jdbconn,server=y,suspend=n";
  }
}
#tack on the classpath, class name, and bin dir
$cmd = $cmd . " -classpath " . $ENV{"CLASSPATH"} . " " . $CLASSNAME . " " . 
  $BINDIR;

# tack on any additional arguments
if ($MIN_ARGC < $ARGC) {
  for ($i = $MIN_ARGC; $i < $ARGC; $i++) {
    $cmd = $cmd . " " . $ARGV[$i];
  }
}

print $cmd . "\n";

system($cmd);
