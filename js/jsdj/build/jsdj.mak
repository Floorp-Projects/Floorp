# jband - 09/14/98 

BUILD_DIR = .
DIST = $(BUILD_DIR)\..\dist
DIST_CLASSES = $(BUILD_DIR)\..\dist\classes
CLASS_SRC = $(BUILD_DIR)\..\classes

JSDEBUGGING_DIR = $(BUILD_DIR)\..\classes\com\netscape\jsdebugging
PALOMAR_DIR = $(JSDEBUGGING_DIR)\ifcui\palomar

IFC_DIST_FILE = $(DIST_CLASSES)\ifc11.jar

!IF "$(NO_RHINO)" == ""
RHINO_CLASSROOT = $(BUILD_DIR)\..\..\jsjava
RHINO_CLASSES = $(RHINO_CLASSROOT)\js.jar;$(RHINO_CLASSROOT)\jsdebug.jar

#RHINO_CLASSES = $(BUILD_DIR)\..\..\..\..\ns\js\rhino
!ENDIF

#these are used for CD
FROM_BUILD_TO_DIST_CLASSES = ..\dist\classes
FROM_DIST_CLASSES_TO_BUILD = ..\..\build

JSLOGGER_JARFILE  = jslogger.jar
JSLOGGER_MAIN_DIR = $(BUILD_DIR)\..\jslogger

NETSCAPE_JSDEBUG_JARFILE  = jsd11.jar

IFCUI_JARFILE  = jsdeb12.jar
IFCUI_RUN_DIR = $(BUILD_DIR)\..\ifcui\run

OUR_CLASSPATH = $(DIST_CLASSES);$(IFC_DIST_FILE);$(RHINO_CLASSES);$(CLASSPATH)
STD_JAVA_FLAGS = -d $(DIST_CLASSES) -classpath $(OUR_CLASSPATH)

CORBA_CLASSPATH = $(OUR_CLASSPATH);$(ES3_ROOT)\wai\java\nisb.zip;$(ES3_ROOT)\wai\java\WAI.zip;$(ES3_ROOT)\plugins\Java\classes\serv3_0.zip
CORBA_JAVA_FLAGS = -d $(DIST_CLASSES) -classpath $(CORBA_CLASSPATH)

!IF "$(NO_CORBA)" == ""
JAVADOC_CLASSPATH = $(CORBA_CLASSPATH)
!ELSE
JAVADOC_CLASSPATH = $(OUR_CLASSPATH)
!ENDIF 


!IF "$(BUILD_OPT)" != ""
JAVAFLAGS = $(STD_JAVA_FLAGS) -O
!ELSE
JAVAFLAGS = $(STD_JAVA_FLAGS) -g
!ENDIF 

all: core jsdb ifcui ifcui_lanchers jslogger apitests tools \
     dependency_checks jars netscape_applet javadoc_all

all_clean : clean all

help :
    @echo targets:
    @echo --------
    @echo all 
    @echo all_clean
    @echo help 
    @echo clean
    @echo core
    @echo jsdb 
    @echo apitests 
    @echo ifcui 
    @echo ifcui_fast 
    @echo ifcui_lanchers 
    @echo jslogger 
    @echo jslogger_fast 
    @echo depend_tool 
    @echo tools 
    @echo dependency_checks 
    @echo jars 
    @echo netscape_security 
    @echo netscape_jsdebug 
    @echo netscape_javascript
    @echo netscape_applet 
    @echo com_netscape_nativejsengine
    @echo com_netscape_jsdebugging_api 
    @echo com_netscape_jsdebugging_engine 
    @echo com_netscape_jsdebugging_api_local 
    @echo com_netscape_jsdebugging_engine_local 
    @echo com_netscape_jsdebugging_api_rhino 
    @echo com_netscape_jsdebugging_engine_rhino 
    @echo com_netscape_jsdebugging_jsdb 
    @echo com_netscape_jsdebugging_ifcui_palomar 
    @echo com_netscape_jsdebugging_ifcui 
    @echo com_netscape_jsdebugging_ifcui_launcher_local 
    @echo com_netscape_jsdebugging_ifcui_launcher_rhino 
    @echo com_netscape_jsdebugging_apitests 
    @echo com_netscape_jsdebugging_jslogger 
    @echo com_netscape_jsdebugging_tools_depend 
    @echo check_depend_jslogger 
    @echo jslogger_jar 
    @echo check_depend_netscape_jsdebug 
    @echo netscape_jsdebug_jar 

core:   initial_state                           \
        netscape_security                       \
        netscape_jsdebug                        \
        com_netscape_jsdebugging_api            \
        com_netscape_jsdebugging_engine         \
        com_netscape_jsdebugging_api_local      \
        com_netscape_jsdebugging_engine_local   \
!IF "$(NO_RHINO)" == ""
        com_netscape_jsdebugging_api_rhino      \
        com_netscape_jsdebugging_engine_rhino   \
!ENDIF
!IF "$(NO_CORBA)" == ""
        com_netscape_jsdebugging_remote_corba   \
        com_netscape_jsdebugging_api_corba      \
!ENDIF

jsdb : com_netscape_jsdebugging_jsdb

apitests : com_netscape_jsdebugging_apitests

ifcui : initial_state                           \
        netscape_security                       \
        netscape_javascript                     \
        netscape_jsdebug                        \
        com_netscape_jsdebugging_api            \
        com_netscape_jsdebugging_ifcui_palomar  \
        com_netscape_jsdebugging_ifcui          \
        CP_DIST_CLASSES_RESOURCES

ifcui_fast : com_netscape_jsdebugging_ifcui

ifcui_lanchers : com_netscape_jsdebugging_ifcui_launcher_local \
!IF "$(NO_RHINO)" == ""
                 com_netscape_jsdebugging_ifcui_launcher_rhino
!ENDIF

jslogger : ifcui com_netscape_jsdebugging_jslogger

jslogger_fast : com_netscape_jsdebugging_jslogger

depend_tool : initial_state com_netscape_jsdebugging_tools_depend

tools : depend_tool

dependency_checks : check_depend_jslogger \
                    check_depend_netscape_jsdebug \
                    check_depend_ifcui

jars : jslogger_jar \
       netscape_jsdebug_jar \
       ifcui_jar

netscape_security : initial_state
    @echo building stubbed netscape.security classes
    @sj $(JAVAFLAGS) $(BUILD_DIR)\..\stub_classes\netscape\security\*.java

netscape_applet : initial_state
    @echo building stubbed netscape.applet classes
    @sj $(JAVAFLAGS) $(BUILD_DIR)\..\stub_classes\netscape\applet\*.java

netscape_jsdebug : netscape_security
    @echo building netscape.jsdebug
    @sj $(JAVAFLAGS) $(BUILD_DIR)\..\classes\netscape\jsdebug\*.java
    
netscape_javascript: netscape_security
    @echo building netscape.javascript
    @sj $(JAVAFLAGS) $(BUILD_DIR)\..\stub_classes\netscape\javascript\*.java

com_netscape_jsdebugging_api : netscape_security
    @echo building com.netscape.jsdebugging.api
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\api\*.java

com_netscape_jsdebugging_engine : com_netscape_jsdebugging_api
    @echo building com.netscape.jsdebugging.engine
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\engine\*.java

com_netscape_jsdebugging_api_local : com_netscape_jsdebugging_api netscape_jsdebug
    @echo building com.netscape.jsdebugging.api.local
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\api\local\*.java

com_netscape_jsdebugging_engine_local : com_netscape_nativejsengine com_netscape_jsdebugging_engine netscape_jsdebug
    @echo building com.netscape.jsdebugging.engine.local
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\engine\local\*.java

com_netscape_nativejsengine : 
    @echo building com.netscape.nativejsengine
    @sj $(JAVAFLAGS) $(CLASS_SRC)\com\netscape\nativejsengine\*.java

com_netscape_jsdebugging_api_rhino : com_netscape_jsdebugging_api
    @echo building com.netscape.jsdebugging.api.rhino
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\api\rhino\*.java

com_netscape_jsdebugging_engine_rhino : com_netscape_jsdebugging_engine
    @echo building com.netscape.jsdebugging.engine.rhino
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\engine\rhino\*.java

com_netscape_jsdebugging_jsdb : core
    @echo building com.netscape.jsdebugging.jsdb
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\jsdb\*.java

com_netscape_jsdebugging_ifcui_palomar : initial_state
    @echo building com.netscape.jsdebugging.ifcui.palomar etc...
    @sj $(JAVAFLAGS)\
            $(PALOMAR_DIR)\util\*.java\
            $(PALOMAR_DIR)\widget\*.java\
            $(PALOMAR_DIR)\widget\layout\*.java\
            $(PALOMAR_DIR)\widget\layout\math\*.java\
            $(PALOMAR_DIR)\widget\toolbar\*.java\
            $(PALOMAR_DIR)\widget\toolTip\*.java

com_netscape_jsdebugging_ifcui :
    @echo generating com.netscape.jsdebugging.ifcui.BuildDate
    @gawk -f $(BUILD_DIR)\gen_date.awk -vpackage_name=com.netscape.jsdebugging.ifcui > $(JSDEBUGGING_DIR)\ifcui\BuildDate.java
    @echo building com.netscape.jsdebugging.ifcui
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\ifcui\*.java

com_netscape_jsdebugging_ifcui_launcher_local : ifcui
    @echo building com.netscape.jsdebugging.ifcui.launcher.local
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\ifcui\launcher\local\*.java

com_netscape_jsdebugging_ifcui_launcher_rhino : com_netscape_jsdebugging_api_rhino ifcui
    @echo building com.netscape.jsdebugging.ifcui.launcher.rhino
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\ifcui\launcher\rhino\*.java

com_netscape_jsdebugging_apitests : core
    @echo building com.netscape.jsdebugging.apitests etc...
    @sj $(JAVAFLAGS)\
            $(JSDEBUGGING_DIR)\apitests\*.java\
            $(JSDEBUGGING_DIR)\apitests\xml\*.java\
            $(JSDEBUGGING_DIR)\apitests\testing\*.java\
            $(JSDEBUGGING_DIR)\apitests\testing\desc\*.java\
            $(JSDEBUGGING_DIR)\apitests\testing\tests\*.java\
            $(JSDEBUGGING_DIR)\apitests\analyzing\*.java\
            $(JSDEBUGGING_DIR)\apitests\analyzing\analyzers\*.java\
            $(JSDEBUGGING_DIR)\apitests\analyzing\data\*.java\
            $(JSDEBUGGING_DIR)\apitests\analyzing\tree\*.java

com_netscape_jsdebugging_jslogger :
    @echo building com.netscape.jsdebugging.jslogger
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\jslogger\*.java

com_netscape_jsdebugging_tools_depend :
    @echo building com.netscape.jsdebugging.tools.depend
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\tools\depend\*.java

com_netscape_jsdebugging_remote_corba : initial_state
    @echo building com.netscape.jsdebugging.remote.corba
    @if "$(ES3_ROOT)" == "" @echo !!! ES3_ROOT is not set !!!
    @if not exist $(ES3_ROOT)\NUL @echo !!! $(ES3_ROOT) does not exist !!!
    @sj $(CORBA_JAVA_FLAGS) \
        $(JSDEBUGGING_DIR)\remote\corba\*.java \
        $(JSDEBUGGING_DIR)\remote\corba\ISourceTextProviderPackage\*.java \
        $(JSDEBUGGING_DIR)\remote\corba\TestInterfacePackage\*.java \

com_netscape_jsdebugging_api_corba : com_netscape_jsdebugging_api com_netscape_jsdebugging_remote_corba ifcui
    @echo building com.netscape.jsdebugging.api.corba
    @sj $(CORBA_JAVA_FLAGS) $(JSDEBUGGING_DIR)\api\corba\*.java

api_corba_fast :
    @echo building com.netscape.jsdebugging.api.corba
    @sj $(CORBA_JAVA_FLAGS) $(JSDEBUGGING_DIR)\api\corba\*.java

palomar_assert_on :
    @echo generating com.netscape.jsdebugging.ifcui.palomar.util with assert on
    @gawk -f $(BUILD_DIR)\gen_dbg.awk -vvalue=true -vpackage_name=com.netscape.jsdebugging.ifcui.palomar.util > $(JSDEBUGGING_DIR)\ifcui\palomar\util\AS.java
    @echo building com.netscape.jsdebugging.ifcui.palomar.util
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\ifcui\palomar\util\*.java

palomar_assert_off :
    @echo generating com.netscape.jsdebugging.ifcui.palomar.util with assert on
    @gawk -f $(BUILD_DIR)\gen_dbg.awk -vvalue=false -vpackage_name=com.netscape.jsdebugging.ifcui.palomar.util > $(JSDEBUGGING_DIR)\ifcui\palomar\util\AS.java
    @echo building com.netscape.jsdebugging.ifcui.palomar.util
    @sj $(JAVAFLAGS) $(JSDEBUGGING_DIR)\ifcui\palomar\util\*.java

########## packaging #################

JSLOGGER_CLASS_FILES = \
    com\netscape\jsdebugging\jslogger\*.class \
    com\netscape\jsdebugging\ifcui\Log.class \
    com\netscape\jsdebugging\ifcui\Env.class \
    com\netscape\jsdebugging\ifcui\palomar\util\*.class

JSLOGGER_EXPECTED_DEPENDENCIES = \
    -i java \
    -i netscape/application \
    -i netscape/util \
    -i netscape/jsdebug \
    -i netscape/security

check_depend_jslogger :
    @echo checking for unexpected dependencies in jslogger
    @cd $(FROM_BUILD_TO_DIST_CLASSES)
    @jre -cp . com.netscape.jsdebugging.tools.depend.Main \
         $(JSLOGGER_CLASS_FILES) $(JSLOGGER_EXPECTED_DEPENDENCIES)
    @cd $(FROM_DIST_CLASSES_TO_BUILD)

jslogger_jar : 
    @echo building $(JSLOGGER_JARFILE)
    @cd $(FROM_BUILD_TO_DIST_CLASSES)
    @if exist $(JSLOGGER_JARFILE) @del $(JSLOGGER_JARFILE) >NUL
    @zip -q -r $(JSLOGGER_JARFILE) $(JSLOGGER_CLASS_FILES)
    @cd $(FROM_DIST_CLASSES_TO_BUILD)
    @echo copying $(JSLOGGER_JARFILE) to $(JSLOGGER_MAIN_DIR)
    @copy $(DIST_CLASSES)\$(JSLOGGER_JARFILE) $(JSLOGGER_MAIN_DIR) >NUL

##########

NETSCAPE_JSDEBUG_CLASS_FILES = netscape\jsdebug\*.class

NETSCAPE_JSDEBUG_EXPECTED_DEPENDENCIES = \
    -i java \
    -i netscape/security \
    -i netscape/util \

check_depend_netscape_jsdebug :
    @echo checking for unexpected dependencies in netscape.jsdebug
    @cd $(FROM_BUILD_TO_DIST_CLASSES)
    @jre -cp . com.netscape.jsdebugging.tools.depend.Main \
         $(NETSCAPE_JSDEBUG_CLASS_FILES) $(NETSCAPE_JSDEBUG_EXPECTED_DEPENDENCIES)
    @cd $(FROM_DIST_CLASSES_TO_BUILD)

netscape_jsdebug_jar : 
    @echo building $(NETSCAPE_JSDEBUG_JARFILE)
    @cd $(FROM_BUILD_TO_DIST_CLASSES)
    @if exist $(NETSCAPE_JSDEBUG_JARFILE) @del $(NETSCAPE_JSDEBUG_JARFILE) >NUL
    @zip -q -r $(NETSCAPE_JSDEBUG_JARFILE) $(NETSCAPE_JSDEBUG_CLASS_FILES)
    @cd $(FROM_DIST_CLASSES_TO_BUILD)
#    @echo copying $(JSLOGGER_JARFILE) to $(JSLOGGER_MAIN_DIR)
#    @copy $(DIST_CLASSES)\$(JSLOGGER_JARFILE) $(JSLOGGER_MAIN_DIR) >NUL

##########

IFCUI_CLASS_FILES = \
        com\netscape\jsdebugging\ifcui\*.class \
        com\netscape\jsdebugging\ifcui\palomar\util\*.class \
        com\netscape\jsdebugging\ifcui\palomar\widget\*.class \
        com\netscape\jsdebugging\ifcui\palomar\widget\layout\*.class \
        com\netscape\jsdebugging\ifcui\palomar\widget\layout\math\*.class \
        com\netscape\jsdebugging\ifcui\palomar\widget\toolbar\*.class \
        com\netscape\jsdebugging\ifcui\palomar\widget\toolTip\*.class \
        com\netscape\jsdebugging\api\*.class \
!IF "$(INCLUDE_SECURITY)" != ""
        netscape\security\*.class \
!ENDIF
!IF "$(INCLUDE_LOCAL_ADAPTER)" != ""
        com\netscape\jsdebugging\api\local\*.class \
!ENDIF
!IF "$(INCLUDE_RHINO_ADAPTER)" != ""
        com\netscape\jsdebugging\api\rhino\*.class \
!ENDIF
!IF "$(INCLUDE_CORBA_ADAPTER)" != ""
        com\netscape\jsdebugging\api\corba\*.class \
!ENDIF
!IF "$(INCLUDE_LOCAL_LAUNCHER)" != ""
        com\netscape\jsdebugging\ifcui\launcher\local\*.class \
!ENDIF
!IF "$(INCLUDE_RHINO_LAUNCHER)" != ""
        com\netscape\jsdebugging\ifcui\launcher\rhino\*.class \
!ENDIF

IFCUI_OTHER_FILES = \
!IF "$(INCLUDE_IMAGES)" != ""
    images\*.gif \
!ENDIF
!IF "$(INCLUDE_SOUNDS)" != ""
    sounds\*.au \
!ENDIF

IFCUI_EXPECTED_DEPENDENCIES = \
    -i java \
    -i netscape/security \
    -i netscape/util \
    -i netscape/application \
    -i netscape/javascript \
!IF "$(INCLUDE_LOCAL_ADAPTER)" != ""
    -i netscape/jsdebug \
!ENDIF
!IF "$(INCLUDE_RHINO_ADAPTER)" != ""
    -i com/netscape/javascript \
!ENDIF

check_depend_ifcui :
    @echo checking for unexpected dependencies for ifcui
    @cd $(FROM_BUILD_TO_DIST_CLASSES)
    @jre -cp . com.netscape.jsdebugging.tools.depend.Main \
         $(IFCUI_CLASS_FILES) $(IFCUI_EXPECTED_DEPENDENCIES)
    @cd $(FROM_DIST_CLASSES_TO_BUILD)

ifcui_jar : 
    @echo building $(IFCUI_JARFILE)
    @cd $(FROM_BUILD_TO_DIST_CLASSES)
    @if exist $(IFCUI_JARFILE) @del $(IFCUI_JARFILE) >NUL
    @zip -q -r $(IFCUI_JARFILE) $(IFCUI_CLASS_FILES) $(IFCUI_OTHER_FILES)
    @cd $(FROM_DIST_CLASSES_TO_BUILD)
    @echo copying $(IFCUI_JARFILE) to $(IFCUI_RUN_DIR)
    @copy $(DIST_CLASSES)\$(IFCUI_JARFILE) $(IFCUI_RUN_DIR) >NUL


########## javadoc stuff #################

ALL_PACKAGES = \
    netscape.security \
    netscape.jsdebug \
    netscape.javascript \
    netscape.applet \
    com.netscape.jsdebugging.api \
    com.netscape.jsdebugging.api.local \
    com.netscape.jsdebugging.engine \
    com.netscape.jsdebugging.engine.local \
    com.netscape.jsdebugging.jsdb \
    com.netscape.jsdebugging.ifcui \
    com.netscape.jsdebugging.ifcui.palomar.util \
    com.netscape.jsdebugging.ifcui.palomar.widget \
    com.netscape.jsdebugging.ifcui.palomar.widget.layout \
    com.netscape.jsdebugging.ifcui.palomar.widget.layout.math \
    com.netscape.jsdebugging.ifcui.palomar.widget.toolbar \
    com.netscape.jsdebugging.ifcui.palomar.widget.toolTip \
    com.netscape.jsdebugging.ifcui.launcher.local \
    com.netscape.jsdebugging.apitests \
    com.netscape.jsdebugging.apitests.xml \
    com.netscape.jsdebugging.apitests.testing \
    com.netscape.jsdebugging.apitests.testing.desc \
    com.netscape.jsdebugging.apitests.testing.tests \
    com.netscape.jsdebugging.apitests.analyzing.analyzers \
    com.netscape.jsdebugging.apitests.analyzing.data \
    com.netscape.jsdebugging.apitests.analyzing.tree \
    com.netscape.jsdebugging.jslogger \
    com.netscape.jsdebugging.tools.depend \
    com.netscape.nativejsengine \
!IF "$(NO_RHINO)" == ""
    com.netscape.jsdebugging.api.rhino \
    com.netscape.jsdebugging.engine.rhino \
    com.netscape.jsdebugging.ifcui.launcher.rhino \
!ENDIF
!IF "$(NO_CORBA)" == ""
    com.netscape.jsdebugging.api.corba \
    com.netscape.jsdebugging.remote.corba \
    com.netscape.jsdebugging.remote.corba.ISourceTextProviderPackage \
    com.netscape.jsdebugging.remote.corba.TestInterfacePackage \
!ENDIF

javadoc_all : initial_state
    @echo cleaning up old stuff in javadoc directory
    @if exist $(DIST)\javadocs\*.html @del $(DIST)\javadocs\*.html > NUL
    @echo copying javadoc images to the dist dir
    @copy $(BUILD_DIR)\images\*.gif  $(DIST)\javadocs\images > NUL
    @echo building javadoc for EVERYTHING
    @javadoc -sourcepath $(JAVADOC_CLASSPATH);$(CLASS_SRC);$(BUILD_DIR)\..\stub_classes -d $(DIST)\javadocs $(ALL_PACKAGES)


########## before anything can happen... #################

initial_state : MK_DIST_CLASSES_DIRS $(IFC_DIST_FILE)

$(IFC_DIST_FILE) : 
    @echo getting ifc11.jar -- if this fails you need to copy it to dist/classes yourself
    copy ..\..\..\nav-java\stubs\classes\ifc11.jar $(DIST_CLASSES) > NUL

########## directory stuff #################

#
# This list got too long so I split it in two. If you need to add more items
# then add them to the second list...
#
DIST_CLASSES_DIRS_0 = \
    $(DIST) \
    $(DIST_CLASSES) \
    $(DIST_CLASSES)\images \
    $(DIST_CLASSES)\sounds \
    $(DIST_CLASSES)\netscape \
    $(DIST_CLASSES)\netscape\security \
    $(DIST_CLASSES)\netscape\jsdebug \
    $(DIST_CLASSES)\netscape\javascript \
    $(DIST_CLASSES)\netscape\applet \
    $(DIST_CLASSES)\com \
    $(DIST_CLASSES)\com\netscape \
    $(DIST_CLASSES)\com\netscape\jsdebugging \
    $(DIST_CLASSES)\com\netscape\jsdebugging\api \
    $(DIST_CLASSES)\com\netscape\jsdebugging\api\local \
    $(DIST_CLASSES)\com\netscape\jsdebugging\api\rhino \
    $(DIST_CLASSES)\com\netscape\jsdebugging\api\corba \
    $(DIST_CLASSES)\com\netscape\jsdebugging\engine \
    $(DIST_CLASSES)\com\netscape\jsdebugging\engine\local \
    $(DIST_CLASSES)\com\netscape\jsdebugging\engine\rhino \
    $(DIST_CLASSES)\com\netscape\jsdebugging\jsdb \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar\util \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar\widget \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar\widget\layout \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar\widget\layout\math \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar\widget\toolbar \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\palomar\widget\toolTip \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\launcher \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\launcher\local \
    $(DIST_CLASSES)\com\netscape\jsdebugging\ifcui\launcher\rhino \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\xml \

DIST_CLASSES_DIRS_1 = \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\testing \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\testing\desc \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\testing\tests \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\analyzing \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\analyzing\analyzers \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\analyzing\data \
    $(DIST_CLASSES)\com\netscape\jsdebugging\apitests\analyzing\tree \
    $(DIST_CLASSES)\com\netscape\jsdebugging\jslogger \
    $(DIST_CLASSES)\com\netscape\jsdebugging\tools \
    $(DIST_CLASSES)\com\netscape\jsdebugging\tools\depend \
    $(DIST_CLASSES)\com\netscape\nativejsengine \
    $(DIST)\javadocs \
    $(DIST)\javadocs\images \
    $(DIST_CLASSES)\com\netscape\jsdebugging\remote \
    $(DIST_CLASSES)\com\netscape\jsdebugging\remote\corba \
    $(DIST_CLASSES)\com\netscape\jsdebugging\remote\corba\ISourceTextProviderPackage \
    $(DIST_CLASSES)\com\netscape\jsdebugging\remote\corba\TestInterfacePackage \


MK_DIST_CLASSES_DIRS :
    @echo making any non existent classfile output directories
    @for %i in ($(DIST_CLASSES_DIRS_0)) do @if not exist %i\NUL mkdir %i > NUL
    @for %i in ($(DIST_CLASSES_DIRS_1)) do @if not exist %i\NUL mkdir %i > NUL

CP_DIST_CLASSES_RESOURCES : initial_state
    @echo copying images and sounds to dist directory
    @xcopy $(BUILD_DIR)\..\ifcui\run\images\*.* $(DIST_CLASSES)\images > NUL
    @xcopy $(BUILD_DIR)\..\ifcui\run\sounds\*.* $(DIST_CLASSES)\sounds > NUL

clean:
    @echo deleting built classes
    @for %i in ($(DIST_CLASSES_DIRS_0)) do @if exist %i\*.class @del %i\*.class > NUL
    @for %i in ($(DIST_CLASSES_DIRS_1)) do @if exist %i\*.class @del %i\*.class > NUL
    @echo deleting ifcui image and sound resources
    @if exist $(DIST_CLASSES)\images\*.gif @del $(DIST_CLASSES)\images\*.gif > NUL
    @if exist $(DIST_CLASSES)\sounds\*.au @del $(DIST_CLASSES)\sounds\*.au > NUL
