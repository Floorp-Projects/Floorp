# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
# Copyright (C) 2010-2014, International Business Machines Corporation and others.
# All Rights Reserved.                  
#
# Commands for regenerating ICU4C locale data (.txt files) from CLDR,
# updated to apply to CLDR 37 / ICU 67 and later versions.
#
# The process requires local copies of
#    - CLDR (the source of most of the data, and some Java tools)
#    - The complete ICU source tree, including:
#      tools - includes the LdmlConverter build tool and associated config files
#      icu4c - the target for converted CLDR data, and source for ICU4J data;
#              includes tests for the converted data
#      icu4j - the target for updated data jars; includes tests for the converted
#              data
#
# For an official CLDR data integration into ICU, these should be clean, freshly
# checked-out. For released CLDR sources, an alternative to checking out sources
# for a given version is downloading the zipped sources for the common (core.zip)
# and tools (tools.zip) directory subtrees from the Data column in
# [http://cldr.unicode.org/index/downloads].
#
# The versions of each of these must match. Included with the release notes for
# ICU is the version number and/or a CLDR git tag name for the revision of CLDR
# that was the source of the data for that release of ICU.
#
# Besides a standard JDK, the process also requires ant and maven
# (http://ant.apache.org/),
# plus the xml-apis.jar from the Apache xalan package
# (http://xml.apache.org/xalan-j/downloads.html).
#
# Note: Enough things can (and will) fail in this process that it is best to
#   run the commands separately from an interactive shell. They should all
#   copy and paste without problems.
#
# It is often useful to save logs of the output of many of the steps in this
# process. The commands below put log files in /tmp; you may want to put them
# somewhere else.
#
#----
#
# There are several environment variables that need to be defined.
#
# a) Java- and ant-related variables
#
# JAVA_HOME:     Path to JDK (a directory, containing e.g. bin/java, bin/javac,
#                etc.); on many systems this can be set using
#                `/usr/libexec/java_home`.
#
# ANT_OPTS:      You may want to set:
#
#                -Xmx4096m, to give Java more memory; otherwise it may run out
#                 of heap.
#
# b) CLDR-related variables
#
# CLDR_DIR:      For most of the process, this is the path to the to root of
#                standard CLDR sources, below which are the common and
#                tools directories. For running LdmlConverter, this needs to be
#                temporarily reset to the parallel root for the production data,
#                corresponding to $CLDR_TMP_DIR/production (see description of
#                CLDR_TMP_DIR below).
# CLDR_CLASSES:  Path to the CLDR Tools classes directory. If not set, defaults
#                to $CLDR_DIR/tools/java/classes
#
# CLDR_TMP_DIR:  Parent of temporary CLDR production data.
#                Defaults to $CLDR_DIR/../cldr-aux (sibling to CLDR_DIR).
#
#                *** NOTE ***: In CLDR 36 and 37, the GenerateProductionData tool
#                no longer generates data by default into $CLDR_TMP_DIR/production;
#                instead it generates data into $CLDR_DIR/../cldr-staging/production
#                (though there is a command-line option to override this). However
#                the rest of the build still assumes that the generated data is in
#                $CLDR_TMP_DIR/production. So CLDR_TMP_DIR must be defined to be
#                $CLDR_DIR/../cldr-staging
#
# c) ICU-related variables
# These variables only need to be set if you're directly reusing the
# commands below.
#
# ICU4C_DIR:     Path to root of ICU4C sources, below which is the source dir.
#
# ICU4J_ROOT:    Path to root of ICU4J sources, below which is the main dir.
#
# TOOLS_ROOT:    Path to root of ICU tools directory, below which is (e.g.) the
#                cldr and unicodetools dirs.
#
#----
#
# If you are adding or removing locales, or specific kinds of locale data,
# there are some xml files in the ICU sources that need to be updated (these xml
# files are used in addition to the CLDR files as inputs to the CLDR data build
# process for ICU):
#
# The primary file to edit for ICU 67 and later is
#
#    $TOOLS_ROOT/cldr/cldr-to-icu/build-icu-data.xml
#
# The files used in previous versions are the following. These are not used in
# the ICU-67-and-later process, but for now they should be kept up to date to
# enable parallel use of the older build process for verification.
#
#    $ICU4C_DIR/source/data/icu-config.xml - Update <locales> to add or remove
#                CLDR locales for inclusion in ICU. Update <paths> to prefer
#                alt forms for certain paths, or to exclude certain paths; note
#                that <paths> items can only have draft or alt attributes.
#
#                Note that if a language-only locale (e.g. "de") is included in
#                <locales>, then all region sublocales for that language that
#                are present in CLDR data (e.g. "de_AT", "de_BE", "de_CH", etc.)
#                should also be included in <locales>, per PMC policy decision
#                2012-05-02 (see http://bugs.icu-project.org/trac/ticket/9298).
#
#    $ICU4C_DIR/source/data/build.xml - If you are adding or removing break
#                iterators, you need to update  <fileset id="brkitr" ...> under
#                <target name="clean" ...> to clean the correct set of files.
#
#                If there are new CLDR resource bundle types, you may need to
#                updated the <remapper> sections to put these in the correct
#                data subfolder for ICU.
#
#    $ICU4C_DIR/source/data/xml/      - If you are adding a new locale, break
#                iterator, collation tailoring, or rule-based number formatter,
#                you may need to add a corresponding xml file in (respectively)
#                the main/, brkitr/, collation/, or rbnf/ subdirectory here.
#
#----
#
# For an official CLDR data integration into ICU, there are some additional
# considerations:
#
# a) Don't commit anything in ICU sources (and possibly any changes in CLDR
#    sources, depending on their nature) until you have finished testing and
#    resolving build issues and test failures for both ICU4C and ICU4J.
#
# b) There are version numbers that may need manual updating in CLDR (other
#    version numbers get updated automatically, based on these):
#
#    common/dtd/ldml.dtd                            - update cldrVersion
#    common/dtd/ldmlBCP47.dtd                       - update cldrVersion
#    common/dtd/ldmlSupplemental.dtd                - update cldrVersion
#    common/dtd/ldmlSupplemental.dtd                - updateunicodeVersion
#    keyboards/dtd/ldmlKeyboard.dtd                 - update cldrVersion
#    tools/java/org/unicode/cldr/util/CLDRFile.java - update GEN_VERSION
#
# c) After everything is committed, you will need to tag the CLDR and ICU
#    sources that ended up being used for the integration; see step 16
#    below.
#
################################################################################

# 1a. Java and ant variables, adjust for your system

export JAVA_HOME=`/usr/libexec/java_home`
export ANT_OPTS="-Xmx4096m

# 1b. CLDR variables, adjust for your setup; with cygwin it might be e.g.
# CLDR_DIR=`cygpath -wp /build/cldr`

export CLDR_DIR=$HOME/cldr-myfork

# 1c. ICU variables

export ICU4C_DIR=$HOME/icu-myfork/icu4c
export ICU4J_ROOT=$HOME/icu-myfork/icu4j
export TOOLS_ROOT=$HOME/icu-myfork/tools


# 2. Build the CLDR Java tools and jar

cd $CLDR_DIR/tools/java
ant clean
ant all
ant jar

# 2a. Copy the CLDR jars into $TOOLS_ROOT/cldr/cldr-to-icu/lib/ maven repository;
# see $TOOLS_ROOT/cldr/cldr-to-icu/lib/README.txt

cd $TOOLS_ROOT/cldr/cldr-to-icu/lib/
mvn install:install-file \
  -DgroupId=org.unicode.cldr \
  -DartifactId=cldr-api \
  -Dversion=0.1-SNAPSHOT \
  -Dpackaging=jar \
  -DgeneratePom=true \
  -DlocalRepositoryPath=. \
  -Dfile=$CLDR_DIR/tools/java/cldr.jar

cd $TOOLS_ROOT/cldr/cldr-to-icu/lib/
mvn install:install-file \
  -DgroupId=com.ibm.icu \
  -DartifactId=icu-utilities \
  -Dversion=0.1-SNAPSHOT \
  -Dpackaging=jar \
  -DgeneratePom=true \
  -DlocalRepositoryPath=. \
  -Dfile=$CLDR_DIR/tools/java/libs/utilities.jar

cd $TOOLS_ROOT/cldr/cldr-to-icu/
mvn dependency:purge-local-repository -DsnapshotsOnly=true

# 3. Configure ICU4C, build and test without new data first, to verify that
# there are no pre-existing errors. Here <platform> is the runConfigureICU
# code for the platform you are building, e.g. Linux, MacOSX, Cygwin.

cd $ICU4C_DIR/source
./runConfigureICU <platform>
make clean
make check 2>&1 | tee /tmp/icu4c-oldData-makeCheck.txt

# 4a. Generate the CLDR production data. This process uses ant with ICU's
# data/build.xml
#
# Running "ant cleanprod" is necessary to clean out the production data directory
# (usually $CLDR_TMP_DIR/production ), required if any CLDR data has changed.
#
# Running "ant setup" is not required, but it will print useful errors to
# debug issues with your path when it fails.

cd $ICU4C_DIR/source/data
ant cleanprod
ant setup
ant proddata 2>&1 | tee /tmp/cldr-newData-proddataLog.txt

# 4b. Build the new ICU4C data files; these include .txt files and .py files.
# These new files will replace whatever was already present in the ICU4C sources.
# This process uses the LdmlConverter in $TOOLS_ROOT/cldr/cldr-to-icu/;
# see $TOOLS_ROOT/cldr/cldr-to-icu/README.txt
#
# This process will take several minutes, during most of which there will be no log
# output (so do not assume nothing is happening). Keep a log so you can investigate
# anything that looks suspicious.
#
# This also requires temporarily redefining CLDR_DIR.
#
# Note that "ant clean" should not be run before this. The build-icu-data.xml process
# will automatically run its own "clean" step to delete files it cannot determine to
# be ones that it would generate, except for pasts listed in <retain> elements such as
# coll/de__PHONEBOOK.txt, coll/de_.txt, etc.
#
# Before this step, make any necessary changes to
# build-icu-data.xml$TOOLS_ROOT/cldr/cldr-to-icu/ to add new locales, etc.

cd $TOOLS_ROOT/cldr/cldr-to-icu/
CLDR_DIR=$CLDR_TMP_DIR/production ant -f build-icu-data.xml | tee /tmp/cldr-newData-builddataLog.txt

# 5. Check which data files have modifications, which have been added or removed
# (if there are no changes, you may not need to proceed further). Make sure the
# list seems reasonable.

cd $ICU4C_DIR/source/data
git status

# 5a. You may also want to check which files were modified in CLDR production data:

cd $CLDR_TMP_DIR
git status

# 6. Fix any errors, investigate any warnings.
#
# Fixing may entail modifying CLDR source data or TOOLS_ROOT config files or
# tooling.

# 7. Now rebuild ICU4C with the new data and run make check tests.
# Again, keep a log so you can investigate the errors.
cd $ICU4C_DIR/source

# 7a. If any files were added or removed (likely), re-run configure:
./runConfigureICU <platform>
make clean

# 7b. Now do the rebuild.
make check 2>&1 | tee /tmp/icu4c-newData-makeCheck.txt

# 8. Investigate each test case failure. The first run processing new CLDR data
# from the Survey Tool can result in thousands of failures (in many cases, one
# CLDR data fix can resolve hundreds of test failures). If the error is caused
# by bad CLDR data, then file a CLDR bug, fix the data, and regenerate from
# step 4. If the data is OK but the testcase needs to be updated because the
# data has legitimately changed, then update the testcase. You will check in
# the updated testcases along with the new ICU data at the end of this process.
# Note that if the new data has any differences in structure, you will have to
# update test/testdata/structLocale.txt or /tsutil/cldrtest/TestLocaleStructure
# may fail.
# Repeat steps 4-7 until there are no errors.

# 9. You can also run the make check tests in exhaustive mode (these will also
# be run automatically on the master branch after the merge resulting from this
# integration):

cd $ICU4C_DIR/source
export INTLTEST_OPTS="-e"
export CINTLTST_OPTS="-e"
make check 2>&1 | tee /tmp/icu4c-newData-makeCheckEx.txt

# 10. Again, investigate each failure, fixing CLDR data or ICU test cases as
# appropriate, and repeating steps 4-7 and 9 until there are no errors.

# 11. Now with ICU4J, build and test without new data first, to verify that
# there are no pre-existing errors (or at least to have the pre-existing errors
# as a base for comparison):

cd $ICU4J_ROOT
ant all 2>&1 | tee /tmp/icu4j-oldData-antAll.txt
ant check 2>&1 | tee /tmp/icu4j-oldData-antCheck.txt

# 12. Transfer the data to ICU4J:
cd $ICU4C_DIR/source

# 12a. You need to reconfigure ICU4C to include the unicore data.
ICU_DATA_BUILDTOOL_OPTS=--include_uni_core_data ./runConfigureICU <platform>

# 12b. Now build the jar files.
cd $ICU4C_DIR/source/data
# The following 2 lines are required to include the unicore data:
make clean
make -j6
make icu4j-data-install
cd $ICU4C_DIR/source/test/testdata
make icu4j-data-install

# 13. Now rebuild ICU4J with the new data and run tests:
# Keep a log so you can investigate the errors.

cd $ICU4J_ROOT
ant check 2>&1 | tee /tmp/icu4j-newData-antCheck.txt

# 14. Investigate test case failures; fix test cases and repeat from step 12,
# or fix CLDR data and repeat from step 4, as appropriate, until there are no
# more failures in ICU4C or ICU4J (except failures that were present before you
# began testing the new CLDR data).

# Note that certain data changes and related test failures may require the
# rebuilding of other kinds of data. For example:
# a) Changes to locale matching data may cause failures in e.g. the following:
#      com.ibm.icu.dev.test.util.LocaleDistanceTest (testLoadedDataSameAsBuiltFromScratch)
#      com.ibm.icu.dev.test.util.LocaleMatcherTest (testLikelySubtagsLoadedDataSameAsBuiltFromScratch)
#    To address these requires building and running the tool
#      icu4j/tools/misc/src/com/ibm/icu/dev/tool/locale/LocaleDistanceBuilder.java
#    to regenerate the file icu4c/source/data/misc/langInfo.txt and then regenerating
#    the ICU4J data jars.
# b) Changes to plurals data may cause failures in e.g. the following
#      com.ibm.icu.dev.test.format.PluralRulesTest (TestLocales)
#    To address these requires updating the LOCALE_SNAPSHOT data in
#      icu4j/main/tests/core/src/com/ibm/icu/dev/test/format/PluralRulesTest.java
#    by modifying the TestLocales() test there to run generateLOCALE_SNAPSHOT() and then
#    copying in the updated data.

# 15. Check the file changes; then git add or git rm as necessary, and
# commit the changes.

cd $HOME/icu/
cd ..
git status
# git add or remove as necessary
# commit

# 16. For an official CLDR data integration into ICU, now tag the CLDR and
# ICU sources with an appropriate CLDR milestone (you can check previous
# tags for format), e.g.:

cd $CLDR_DIR
git tag ...
git push --tags

cd $HOME/icu
git tag ...
git push --tags

# 17. You should also commit and tag the update production data in CLDR_TMP_DIR
# using the same tag as for CLDR_DIR above:

cd $CLDR_TMP_DIR
# git add or remove as necessary
# commit
git tag ...
git push --tags



