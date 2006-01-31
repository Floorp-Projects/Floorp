#!/bin/sh

# Set up locale.
if [ -z "$LOCALE" ]; then LOCALE=$1; fi

# Set up paths for finding files.
if [ -z "$FEDIR" ]; then FEDIR=$PWD/..; fi
if [ -z "$CONFIGDIR" ]; then CONFIGDIR=$FEDIR/../../config; fi
if [ -z "$XPILOCALEFILES" ]; then XPILOCALEFILES=$PWD/locale-resources; fi
if [ -z "$XPIROOT" ]; then XPIROOT=$PWD/xpi-tree-$LOCALE; fi
if [ -z "$JARROOT" ]; then JARROOT=$PWD/jar-tree; fi
if [ -z "$PERL" ]; then PERL=perl; fi
if [ -z "$DEBUG" ]; then DEBUG=0; fi

function showParams()
{
  I=0
  for P in "$@"; do
    I=$((I+1))
    echo PARAM $I: "$P"
  done
}

## Call this with lots of parameters to run a command, log errors, and abort
## if it fails. Supports redirection if '>' and '<' are passed as arguments,
## e.g.:
##   safeCommand cmd arg1 arg2 '<' input.file '>' output-file
##
## Note: only a single input and single output redirection is supported.
##
function safeCommand()
{
  local -a CMD
  CMD_COUNT=$((0))
  INF=""
  OUTF=""
  LASTP=""
  for P in "$@"; do
    if [ "$LASTP" = "<" ]; then
      if [ -n "$INF" ]; then
        echo "ERROR: Multiple input files passed to safeCommand()." >&2
        exit 2
      fi
      INF="$P"
    elif [ "$LASTP" = ">" ]; then
      if [ -n "$OUTF" ]; then
        echo "ERROR: Multiple output files passed to safeCommand()." >&2
        exit 2
      fi
      OUTF="$P"
    elif [ "$P" = ">" -o "$P" = "<" ]; then
      echo >/dev/null
    else
      CMD[$CMD_COUNT]="$P"
      CMD_COUNT=$((CMD_COUNT+1))
    fi
    LASTP="$P"
  done
  
  if [ $DEBUG -gt 0 ]; then
    echo
    showParams "${CMD[@]}"
    echo 'INPUT  :' "$INF"
    echo 'OUTPUT :' "$OUTF"
  fi
  
  touch log.stdout log.stderr
  if [ -z "$INF" -a -z "$OUTF" ]; then
    "${CMD[@]}" 1>log.stdout 2>log.stderr
  elif [ -z "$INF" ]; then
    "${CMD[@]}" 1> "$OUTF" 2>log.stderr
  elif [ -z "$OUTF" ]; then
    "${CMD[@]}" < "$INF" 1>log.stdout 2>log.stderr
  else
    "${CMD[@]}" < "$INF" 1> "$OUTF" 2>log.stderr
  fi
  
  EC=$?
  if [ $DEBUG -gt 0 ]; then
    echo 'RESULT :' $EC
  fi
  if [ "$EC" != "0" ]; then
    echo "ERROR ($EC)"
    cat log.stdout
    cat log.stderr
    rm -f log.stdout log.stderr
    exit 1
  fi
  rm -f log.stdout log.stderr
  return $EC
}


## Begin real program ##


if [ "$1" = "clean" ]; then
  echo -n "Cleaning up files"
  echo -n .
  rm -rf "$XPIROOT"
  echo -n .
  rm -rf "$JARROOT"
  echo   ". done."
  
  exit
fi


# Check setup.
if ! [ -d "$FEDIR" ]; then
  echo "ERROR: Base ChatZilla directory (FEDIR) not found."
  exit 1
fi
if ! [ -d "$CONFIGDIR" ]; then
  echo "ERROR: mozilla/config directory (CONFIGDIR) not found."
  exit 1
fi

if [ -z "$LOCALE" ]; then
  echo "ERROR: You need to provide a locale identifier (ab-CD)"
  exit 1
fi


# Extract version number.
VERSION=`grep "const __cz_version" "$FEDIR/xul/content/static.js" | sed "s|.*\"\([^\"]\{1,\}\)\".*|\1|"`

if [ -z "$VERSION" ]; then
  echo "ERROR: Unable to get version number."
  exit 1
fi

echo "Beginning build of $LOCALE locale for ChatZilla $VERSION..."


# Check for existing.
if [ -r "chatzilla-$LOCALE-$VERSION.xpi" ]; then
  echo "  WARNING: output XPI will be overwritten."
fi


# Check for required directory layouts.
echo -n "  Checking XPI structure"
echo -n .
if ! [ -d "xpi-tree-$LOCALE" ]; then mkdir "xpi-tree-$LOCALE"; fi
echo -n .
if ! [ -d "xpi-tree-$LOCALE/chrome" ]; then mkdir "xpi-tree-$LOCALE/chrome"; fi
echo   ".                        done"

echo -n "  Checking JAR structure"
echo -n .
if ! [ -d jar-tree ]; then mkdir jar-tree; fi
echo   ".                         done"


# Make Firefox updates.
echo -n "  Updating Firefox Extension files"
echo -n .
safeCommand sed "--expression=s|@REVISION@|$VERSION|g" "--expression=s|@LOCALE@|$LOCALE|g" '<' "$XPILOCALEFILES/$LOCALE/install.rdf" '>' "$XPIROOT/install.rdf"
echo -n .
echo   ".              done"


# Make Mozilla Suite updates.
echo -n "  Updating Mozilla Extension files"
echo -n .
safeCommand sed "--expression=s|@REVISION@|$VERSION|g" "--expression=s|@LOCALE@|$LOCALE|g" '<' "$XPILOCALEFILES/$LOCALE/install.js" '>' "$XPIROOT/install.js"
echo -n .
safeCommand mv "$FEDIR/xul/locale/en-US/contents.rdf" "$FEDIR/xul/locale/en-US/contents.rdf.in"
safeCommand sed "s|@MOZILLA_VERSION@|cz-$VERSION|g" '<' "$FEDIR/xul/locale/en-US/contents.rdf.in" '>' "$FEDIR/xul/locale/en-US/contents.rdf"
safeCommand rm "$FEDIR/xul/locale/en-US/contents.rdf.in"
echo   ".              done"


# Create JAR.
echo -n "  Constructing JAR package"
echo -n .
OLDPWD=`pwd`
cd "$CONFIGDIR"
echo -n .

safeCommand sed "s|@LOCALE@|$LOCALE|g" '<' "$FEDIR/locale-jar.mn" '>' "$FEDIR/$LOCALE-jar.mn"
echo -n .
safeCommand $PERL make-jars.pl -v -z zip -p preprocessor.pl -s "$FEDIR" -d "$JARROOT" '<' "$FEDIR/$LOCALE-jar.mn"
echo -n .
cd "$OLDPWD"
echo   ".                    done"


# Make XPI.
echo -n "  Constructing XPI package"
echo -n .
safeCommand cp -v "$JARROOT/chatzilla-$LOCALE.jar" "$XPIROOT/chrome/"
echo -n .
safeCommand chmod 664 "$XPIROOT/chrome/chatzilla-$LOCALE.jar"
echo -n .
OLDPWD=`pwd`
cd "$XPIROOT"
safeCommand zip -vr ../chatzilla-$LOCALE-$VERSION.xpi . -i "*" -x log*
cd "$OLDPWD"
echo   ".                     done"


echo "Build of $LOCALE locale for ChatZilla $VERSION... ALL DONE"
