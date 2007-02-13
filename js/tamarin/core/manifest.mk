# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is [Open Source Virtual Machine].
#
# The Initial Developer of the Original Code is
# Adobe System Incorporated.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
