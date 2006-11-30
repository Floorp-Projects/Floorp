STATIC_LIBRARIES += MMgc
MMgc_BUILD_ALL = 1

MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/MMgc.cpp \
  $(curdir)/FixedAlloc.cpp \
  $(curdir)/FixedMalloc.cpp \
  $(curdir)/GC.cpp \
  $(curdir)/GCAlloc.cpp \
  $(curdir)/GCHashtable.cpp \
  $(curdir)/GCHeap.cpp \
  $(curdir)/GCLargeAlloc.cpp \
  $(curdir)/GCMemoryProfiler.cpp \
  $(curdir)/GCObject.cpp \
  $(curdir)/GCTests.cpp \
  $(NULL)

ifeq (windows,$(TARGET_OS))
MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/GCAllocObjectWin.cpp \
  $(curdir)/GCDebugWin.cpp \
  $(curdir)/GCHeapWin.cpp \
  $(NULL)
endif

ifeq (darwin,$(TARGET_OS))
MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/GCAllocObjectMac.cpp \
  $(curdir)/GCDebugMac.cpp \
  $(curdir)/GCHeapMac.cpp \
  $(NULL)
endif

ifeq (linux,$(TARGET_OS))
MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/GCAllocObjectUnix.cpp \
  $(curdir)/GCDebugUnix.cpp \
  $(curdir)/GCHeapUnix.cpp \
  $(NULL)
endif

$(curdir)/GCDebugMac.$(OBJ_SUFFIX): CXXFLAGS += -Wno-deprecated-declarations
