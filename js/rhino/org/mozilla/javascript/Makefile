#! gmake
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Rhino code, released
# May 6, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.

#
# Makefile the core javascript classes.
#
# This Makefile is intended to be called from the toplevel Makefile.
#

# List files explicitly to exclude .java files in this dir we don't want
# to compile.  Also hack in classfile stuff...
SOURCES =						\
	$(PATH_PREFIX)/Arguments.java			\
	$(PATH_PREFIX)/BinaryDigitReader.java		\
	$(PATH_PREFIX)/ClassDefinitionException.java	\
	$(PATH_PREFIX)/ClassNameHelper.java		\
	$(PATH_PREFIX)/Context.java			\
	$(PATH_PREFIX)/DeepBytecodeHook.java		\
	$(PATH_PREFIX)/DeepCallHook.java		\
	$(PATH_PREFIX)/DeepErrorReporterHook.java	\
	$(PATH_PREFIX)/DeepExecuteHook.java		\
	$(PATH_PREFIX)/DeepNewObjectHook.java		\
	$(PATH_PREFIX)/DeepScriptHook.java		\
	$(PATH_PREFIX)/DefaultErrorReporter.java	\
	$(PATH_PREFIX)/EcmaError.java			\
	$(PATH_PREFIX)/ErrorReporter.java		\
	$(PATH_PREFIX)/EvaluatorException.java		\
	$(PATH_PREFIX)/FlattenedObject.java		\
	$(PATH_PREFIX)/Function.java			\
	$(PATH_PREFIX)/FunctionNode.java		\
	$(PATH_PREFIX)/FunctionObject.java		\
	$(PATH_PREFIX)/IRFactory.java			\
	$(PATH_PREFIX)/ImporterTopLevel.java		\
	$(PATH_PREFIX)/InterpretedFunction.java		\
	$(PATH_PREFIX)/InterpretedScript.java		\
	$(PATH_PREFIX)/Interpreter.java			\
	$(PATH_PREFIX)/InterpreterData.java		\
	$(PATH_PREFIX)/JavaAdapter.java			\
	$(PATH_PREFIX)/JavaMembers.java			\
	$(PATH_PREFIX)/JavaScriptException.java		\
	$(PATH_PREFIX)/Label.java			\
	$(PATH_PREFIX)/LabelTable.java			\
	$(PATH_PREFIX)/LazilyLoadedCtor.java		\
	$(PATH_PREFIX)/LineBuffer.java			\
	$(PATH_PREFIX)/ListenerCollection.java		\
	$(PATH_PREFIX)/LocalVariable.java		\
	$(PATH_PREFIX)/NativeArray.java			\
	$(PATH_PREFIX)/NativeBoolean.java		\
	$(PATH_PREFIX)/NativeCall.java			\
	$(PATH_PREFIX)/NativeDate.java			\
	$(PATH_PREFIX)/NativeError.java			\
	$(PATH_PREFIX)/NativeFunction.java		\
	$(PATH_PREFIX)/NativeGlobal.java		\
	$(PATH_PREFIX)/NativeJavaArray.java		\
	$(PATH_PREFIX)/NativeJavaClass.java		\
	$(PATH_PREFIX)/NativeJavaConstructor.java	\
	$(PATH_PREFIX)/NativeJavaMethod.java		\
	$(PATH_PREFIX)/NativeJavaObject.java		\
	$(PATH_PREFIX)/NativeJavaPackage.java		\
	$(PATH_PREFIX)/NativeMath.java			\
	$(PATH_PREFIX)/NativeNumber.java		\
	$(PATH_PREFIX)/NativeObject.java		\
	$(PATH_PREFIX)/NativeScript.java		\
	$(PATH_PREFIX)/NativeString.java		\
	$(PATH_PREFIX)/NativeWith.java			\
	$(PATH_PREFIX)/Node.java			\
	$(PATH_PREFIX)/NodeTransformer.java		\
	$(PATH_PREFIX)/NotAFunctionException.java	\
	$(PATH_PREFIX)/Parser.java			\
	$(PATH_PREFIX)/PreorderNodeIterator.java	\
	$(PATH_PREFIX)/PropertyException.java		\
	$(PATH_PREFIX)/RegExpProxy.java			\
	$(PATH_PREFIX)/Script.java			\
	$(PATH_PREFIX)/ScriptRuntime.java		\
	$(PATH_PREFIX)/Scriptable.java			\
	$(PATH_PREFIX)/ScriptableObject.java		\
	$(PATH_PREFIX)/SecuritySupport.java		\
	$(PATH_PREFIX)/ShallowNodeIterator.java		\
	$(PATH_PREFIX)/SourceTextItem.java		\
	$(PATH_PREFIX)/SourceTextManager.java		\
	$(PATH_PREFIX)/TokenStream.java			\
	$(PATH_PREFIX)/Undefined.java			\
	$(PATH_PREFIX)/VariableTable.java		\
	$(PATH_PREFIX)/WrappedException.java		\
	$(PATH_PREFIX)/Wrapper.java			\
	$(PATH_PREFIX)/regexp/NativeRegExp.java		\
	$(PATH_PREFIX)/regexp/NativeRegExpCtor.java	\
	$(PATH_PREFIX)/regexp/RegExpImpl.java		\
	$(PATH_PREFIX)/regexp/SubString.java		\
	$(PATH_PREFIX)/optimizer/Block.java		\
	$(PATH_PREFIX)/optimizer/Codegen.java		\
	$(PATH_PREFIX)/optimizer/DataFlowBitSet.java	\
	$(PATH_PREFIX)/optimizer/FatBlock.java		\
	$(PATH_PREFIX)/optimizer/JavaScriptClassLoader.java\
	$(PATH_PREFIX)/optimizer/OptClassNameHelper.java\
	$(PATH_PREFIX)/optimizer/OptFunctionNode.java	\
	$(PATH_PREFIX)/optimizer/OptIRFactory.java	\
	$(PATH_PREFIX)/optimizer/OptLocalVariable.java	\
	$(PATH_PREFIX)/optimizer/OptRuntime.java	\
	$(PATH_PREFIX)/optimizer/OptTransformer.java	\
	$(PATH_PREFIX)/optimizer/OptVariableTable.java	\
	$(PATH_PREFIX)/optimizer/Optimizer.java		\
	$(PATH_PREFIX)/optimizer/StmtNodeIterator.java	\
	$(PATH_PREFIX)/optimizer/TypeEvent.java		\
        $(PATH_PREFIX)/debug/DebugReader.java           \
        $(PATH_PREFIX)/debug/DebuggableEngine.java      \
        $(PATH_PREFIX)/debug/DebuggableScript.java      \
        $(PATH_PREFIX)/debug/Debugger.java              \
        $(PATH_PREFIX)/debug/Frame.java                 \
	$(PATH_PREFIX)/../classfile/ByteCode.java	\
	$(PATH_PREFIX)/../classfile/ClassFileWriter.java\
	$(NULL)

RESOURCEDIR = $(PATH_PREFIX)/resources
RESOURCES = $(RESOURCEDIR)/*.properties

# This must be evaluated in some context where the classes can be
# found; we can't use a simple translation from sources, because a
# .java file may produce more than one .class file.
# (use org/mozilla/classfile explicitly for now; should be replaced
# with something parameterized, but jar doesn't understand ..)
CLASSES = $(PATH_PREFIX)/*.class $(PATH_PREFIX)/regexp/*.class \
        $(PATH_PREFIX)/debug/*.class \
	$(PATH_PREFIX)/optimizer/*.class org/mozilla/classfile/*.class

# A class or set of classes as visible from the top level.  For want
# of ${subst ,,}.  This variable is only used to trigger dependency
# analysis, and multiple words confuse gmake, so it can be smaller
# than the full set of sources.  (We assume we'll never need to do the
# same thing with RESOURCES.)
TLCLASS = $(CLASSDIR)/$(PATH_PREFIX)/*.class

# An empty file, used mainly for timestamp/dependency purposes by 
# "fast" builds
FASTTARGET=$(CLASSDIR)/.lastbuild


$(JAR) : $(TLCLASS) $(CLASSDIR)/$(RESOURCES)
	cd $(CLASSDIR) ; \
		jar cf ../$(JAR) $(CLASSES) $(RESOURCES)

$(TLCLASS) : $(SOURCES)
	- mkdir -p $(CLASSDIR)
	echo "" > $(FASTTARGET)
	$(JAVAC) $(JFLAGS) -d $(CLASSDIR) $(SOURCES)

$(CLASSDIR)/$(RESOURCES) : $(RESOURCES)
	- mkdir -p $(CLASSDIR)/$(RESOURCEDIR)
	cp $(RESOURCES) $(CLASSDIR)/$(RESOURCEDIR)

# Since the jar file is a target for regular builds, "fast" builds use a 
# dummy file, updated before each compilation to provide a timestamp.
# Even so, using a dummy file is far from foolproof, so we still need 
# the regular build. 

fast: $(FASTTARGET)

# So that we recompile only the files that have changed, we pretend 
# the only real dependencies are the source files, and recopy the 
# resources every time.  Right now (14 Jun 99), the only resource is 
# Messages.properties, so it's a small price to pay.
$(FASTTARGET) : $(SOURCES)
	- mkdir -p $(CLASSDIR)/$(RESOURCEDIR)
	cp $(RESOURCES) $(CLASSDIR)/$(RESOURCEDIR)
	echo "" > $(FASTTARGET)
	$(JAVAC) $(JFLAGS) -d $(CLASSDIR) $(?)
	cd $(CLASSDIR) ; \
		jar cf ../$(JAR) $(CLASSES) $(RESOURCES)

clean :
	- cd $(CLASSDIR)
	- rm $(CLASSES)
	- rm $(PATH_PREFIX)/message.ids \
		$(PATH_PREFIX)/property.ids \
		$(PATH_PREFIX)/MANIFEST

clobber : clean
	-rm $(JAR)

$(PATH_PREFIX)/MANIFEST : $(SOURCES) $(RESOURCES) $(PATH_PREFIX)/Makefile
	ls $(SOURCES) $(RESOURCES) $(PATH_PREFIX)/Makefile \
		> $(@)

# A sed/grep regular expression.
MESSAGE_PREFIX = msg\.


# Find all the msg.* strings in the source, and condense them to a sorted list,
# excluding duplicates.

$(PATH_PREFIX)/message.ids : $(SOURCES)
	grep '$(MESSAGE_PREFIX)' $(SOURCES) |\
	sed -e 's/.*\"\($(MESSAGE_PREFIX)\)\([^\"]*\).*/\1\2/' | \
	sort | uniq > $(PATH_PREFIX)/message.ids


# Find all the msg.* strings in the resource files, and condense them to a
# sorted list, not excluding duplicates.
$(PATH_PREFIX)/property.ids : $(RESOURCES)
	grep '^$(MESSAGE_PREFIX)' $(RESOURCES) |\
	sed -e 's/.*\($(MESSAGE_PREFIX)\)\([^ =]*\).*/\1\2/' |\
	sort > $(PATH_PREFIX)/property.ids


# Compare the resulting message.ids and property.ids files and confirm
# that they do not differ.  This means that every message string used
# in the source is defined somewhere in the resource file, every
# resource in the resource file is used somewhere in the source, and
# no resource is defined more than once.
check : $(PATH_PREFIX)/message.ids $(PATH_PREFIX)/property.ids FORCE
	- diff $(PATH_PREFIX)/message.ids $(PATH_PREFIX)/property.ids

# look for unmatched single quotes ... seems to fail when none!
#	- sed -e s/\'\'//g $($RESOURCES) | grep \'

# Emulate .PHONY
FORCE :




