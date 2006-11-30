STATIC_LIBRARIES += avmplus
avmplus_BUILD_ALL = 1

avmplus_CXXSRCS := $(avmplus_CXXSRCS) \
  $(curdir)/AbcEnv.cpp \
  $(curdir)/AbcGen.cpp \
  $(curdir)/AbcParser.cpp \
  $(curdir)/AbstractFunction.cpp \
  $(curdir)/ActionBlockConstants.cpp \
  $(curdir)/ArrayClass.cpp \
  $(curdir)/ArrayObject.cpp \
  $(curdir)/AtomArray.cpp \
  $(curdir)/AvmCore.cpp \
  $(curdir)/avmplusDebugger.cpp \
  $(curdir)/avmplusHashtable.cpp \
  $(curdir)/avmplusProfiler.cpp \
  $(curdir)/BigInteger.cpp \
  $(curdir)/BooleanClass.cpp \
  $(curdir)/BuiltinTraits.cpp \
  $(curdir)/ClassClass.cpp \
  $(curdir)/ClassClosure.cpp \
  $(curdir)/DateClass.cpp \
  $(curdir)/DateObject.cpp \
  $(curdir)/Domain.cpp \
  $(curdir)/DomainEnv.cpp \
  $(curdir)/DynamicProfiler.cpp \
  $(curdir)/E4XNode.cpp \
  $(curdir)/ErrorClass.cpp \
  $(curdir)/ErrorConstants.cpp \
  $(curdir)/Exception.cpp \
  $(curdir)/FrameState.cpp \
  $(curdir)/FunctionClass.cpp \
  $(curdir)/GrowableBuffer.cpp \
  $(curdir)/IntClass.cpp \
  $(curdir)/Interpreter.cpp \
  $(curdir)/MathClass.cpp \
  $(curdir)/MathUtils.cpp \
  $(curdir)/MethodClosure.cpp \
  $(curdir)/MethodEnv.cpp \
  $(curdir)/MethodInfo.cpp \
  $(curdir)/Multiname.cpp \
  $(curdir)/MultinameHashtable.cpp \
  $(curdir)/Namespace.cpp \
  $(curdir)/NamespaceClass.cpp \
  $(curdir)/NamespaceSet.cpp \
  $(curdir)/NativeFunction.cpp \
  $(curdir)/NumberClass.cpp \
  $(curdir)/ObjectClass.cpp \
  $(curdir)/opcodes.cpp \
  $(curdir)/PoolObject.cpp \
  $(curdir)/PrintWriter.cpp \
  $(curdir)/RegExpClass.cpp \
  $(curdir)/RegExpObject.cpp \
  $(curdir)/ScopeChain.cpp \
  $(curdir)/ScriptBuffer.cpp \
  $(curdir)/ScriptObject.cpp \
  $(curdir)/StackTrace.cpp \
  $(curdir)/StaticProfiler.cpp \
  $(curdir)/StringBuffer.cpp \
  $(curdir)/StringClass.cpp \
  $(curdir)/StringObject.cpp \
  $(curdir)/Toplevel.cpp \
  $(curdir)/Traits.cpp \
  $(curdir)/UnicodeUtils.cpp \
  $(curdir)/Verifier.cpp \
  $(curdir)/VTable.cpp \
  $(curdir)/XMLClass.cpp \
  $(curdir)/XMLListClass.cpp \
  $(curdir)/XMLListObject.cpp \
  $(curdir)/XMLObject.cpp \
  $(curdir)/XMLParser16.cpp \
  $(curdir)/Date.cpp \
  $(curdir)/AbcData.cpp \
  $(curdir)/AvmPlusScriptableObject.cpp \
  $(NULL)

#  $(curdir)/avmplus.cpp \
#  $(curdir)/AtomConstants.cpp \
