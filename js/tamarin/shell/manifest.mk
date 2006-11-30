PROGRAMS += shell

shell_BASENAME = avmshell
shell_INCLUDES = -I$(srcdir) -I$(topsrcdir)/extensions
shell_DEFINES = -DAVMPLUS_SHELL
shell_STATIC_LIBRARIES = MMgc avmplus
shell_DIR := $(curdir)/

ifdef ENABLE_SHELL
shell_BUILD_ALL = 1
endif

shell_CXXSRCS := $(shell_CXXSRCS) \
  $(curdir)/avmshell.cpp \
  $(curdir)/ByteArrayGlue.cpp \
  $(curdir)/ConsoleOutputStream.cpp \
  $(curdir)/DataIO.cpp \
  $(curdir)/DebugCLI.cpp \
  $(curdir)/DomainClass.cpp \
  $(curdir)/FileClass.cpp \
  $(curdir)/FileInputStream.cpp \
  $(curdir)/StringBuilderClass.cpp \
  $(curdir)/SystemClass.cpp \
  $(curdir)/TypedArrayClass.cpp \
  $(curdir)/../extensions/DictionaryGlue.cpp \
  $(curdir)/../extensions/JavaGlue.cpp \
  $(NULL)
