# this file contains defs that should be in the top level mozilla/config
# directory, but may not be there due to tree update issues.


!ifndef JAVA_HOME
JAVA_HOME=$(JDKHOME)
!endif

JAVA_DESTPATH = $(MOZ_SRC)\mozilla\dist\classes
DEFAULT_JAVA_SOURCEPATH = $(MOZ_SRC)\mozilla\sun-java\classsrc
JAVA_SOURCEPATH = $(MOZ_SRC)\mozilla\sun-java\classsrc11;$(DEFAULT_JAVA_SOURCEPATH)
JAVAC_ZIP=$(JAVA_HOME)\lib\classes.zip
JAVAC_CLASSPATH=$(JAVAC_ZIP)$(PATH_SEPARATOR)$(JAVA_DESTPATH)$(PATH_SEPARATOR)$(JAVA_SOURCEPATH)


JAVA=$(JDKHOME)\bin\java
JAVAH=$(JDKHOME)\bin\JAVAH
JAVAH_FLAGS=-jni -classpath $(JAVAC_CLASSPATH)

