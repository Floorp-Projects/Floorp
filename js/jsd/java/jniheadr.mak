
JSDJNI = .
#CLASS_DIR_BASE = $(JSDJNI)\..\..\..\jsdj\dist\classes
# until jsdj moves to mozilla...
CLASS_DIR_BASE = $(JSDJNI)\..\..\..\..\..\ns\js\jsdj\dist\classes
GEN = $(JSDJNI)\_jni
HEADER_FILE = $(GEN)\jsdjnih.h

PACKAGE_SLASH = netscape\jsdebug
PACKAGE_DOT =   netscape.jsdebug

STD_CLASSPATH = -classpath $(CLASS_DIR_BASE);$(CLASSPATH)

CLASSES_WITH_NATIVES = \
    $(PACKAGE_DOT).DebugController \
    $(PACKAGE_DOT).JSPC \
    $(PACKAGE_DOT).JSSourceTextProvider \
    $(PACKAGE_DOT).JSStackFrameInfo \
    $(PACKAGE_DOT).JSThreadState \
    $(PACKAGE_DOT).Script \
    $(PACKAGE_DOT).SourceTextProvider \
    $(PACKAGE_DOT).ThreadStateBase \
    $(PACKAGE_DOT).Value

all: $(GEN)
    @echo generating JNI headers
    @javah -jni -o "$(HEADER_FILE)" $(STD_CLASSPATH) $(CLASSES_WITH_NATIVES)

$(GEN) :
    @mkdir $(GEN)

clean: 
    @if exist $(HEADER_FILE) @del $(HEADER_FILE) > NUL