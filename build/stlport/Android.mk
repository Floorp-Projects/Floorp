LOCAL_PATH := $(call my-dir)

# Normally, we distribute the NDK with prebuilt binaries of STLport
# in $LOCAL_PATH/libs/<abi>/. However,
#

STLPORT_FORCE_REBUILD := $(strip $(STLPORT_FORCE_REBUILD))
ifndef STLPORT_FORCE_REBUILD
  ifeq (,$(strip $(wildcard $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libstlport_static$(TARGET_LIB_EXTENSION))))
    $(call __ndk_info,WARNING: Rebuilding STLport libraries from sources!)
    $(call __ndk_info,You might want to use $$NDK/build/tools/build-cxx-stl.sh --stl=stlport)
    $(call __ndk_info,in order to build prebuilt versions to speed up your builds!)
    STLPORT_FORCE_REBUILD := true
  endif
endif

libstlport_path := $(LOCAL_PATH)

libstlport_src_files := \
        src/dll_main.cpp \
        src/fstream.cpp \
        src/strstream.cpp \
        src/sstream.cpp \
        src/ios.cpp \
        src/stdio_streambuf.cpp \
        src/istream.cpp \
        src/ostream.cpp \
        src/iostream.cpp \
        src/codecvt.cpp \
        src/collate.cpp \
        src/ctype.cpp \
        src/monetary.cpp \
        src/num_get.cpp \
        src/num_put.cpp \
        src/num_get_float.cpp \
        src/num_put_float.cpp \
        src/numpunct.cpp \
        src/time_facets.cpp \
        src/messages.cpp \
        src/locale.cpp \
        src/locale_impl.cpp \
        src/locale_catalog.cpp \
        src/facets_byname.cpp \
        src/complex.cpp \
        src/complex_io.cpp \
        src/complex_trig.cpp \
        src/string.cpp \
        src/bitset.cpp \
        src/allocators.cpp \
        src/c_locale.c \
        src/cxa.c \

libstlport_cflags := -D_GNU_SOURCE
libstlport_cppflags := -fuse-cxa-atexit
libstlport_c_includes := $(libstlport_path)/stlport

#It is much more practical to include the sources of GAbi++ in our builds
# of STLport. This is similar to what the GNU libstdc++ does (it includes
# its own copy of libsupc++)
#
# This simplifies usage, since you only have to list a single library
# as a dependency, instead of two, especially when using the standalone
# toolchain.
#
include $(dir $(LOCAL_PATH))/gabi++/sources.mk

libstlport_c_includes += $(libgabi++_c_includes)
ifneq ($(strip $(filter-out $(NDK_KNOWN_ARCHS),$(TARGET_ARCH))),)
libgabi++_src_files := src/delete.cc \
                       src/new.cc
endif

ifneq ($(STLPORT_FORCE_REBUILD),true)

$(call ndk_log,Using prebuilt STLport libraries)

include $(CLEAR_VARS)
LOCAL_MODULE := stlport_static
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/lib$(LOCAL_MODULE)$(TARGET_LIB_EXTENSION)
# For armeabi*, choose thumb mode unless LOCAL_ARM_MODE := arm
ifneq (,$(filter armeabi%,$(TARGET_ARCH_ABI)))
ifneq (arm,$(LOCAL_ARM_MODE))
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/thumb/lib$(LOCAL_MODULE)$(TARGET_LIB_EXTENSION)
endif
endif
LOCAL_EXPORT_C_INCLUDES := $(libstlport_c_includes)
LOCAL_CPP_FEATURES := rtti
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := stlport_shared
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/lib$(LOCAL_MODULE)$(TARGET_SONAME_EXTENSION)
# For armeabi*, choose thumb mode unless LOCAL_ARM_MODE := arm
$(info TARGET_ARCH_ABI=$(TARGET_ARCH_ABI))
$(info LOCAL_ARM_MODE=$(LOCAL_ARM_MODE))
ifneq (,$(filter armeabi%,$(TARGET_ARCH_ABI)))
ifneq (arm,$(LOCAL_ARM_MODE))
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/thumb/lib$(LOCAL_MODULE)$(TARGET_SONAME_EXTENSION)
endif
endif
LOCAL_EXPORT_C_INCLUDES := $(libstlport_c_includes)
LOCAL_CPP_FEATURES := rtti
include $(PREBUILT_SHARED_LIBRARY)

else # STLPORT_FORCE_REBUILD == true

$(call ndk_log,Rebuilding STLport libraries from sources)

include $(CLEAR_VARS)
LOCAL_MODULE := stlport_static
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SRC_FILES := $(libstlport_src_files)
LOCAL_SRC_FILES += $(libgabi++_src_files:%=../gabi++/%)
LOCAL_CFLAGS := $(libstlport_cflags)
LOCAL_CPPFLAGS := $(libstlport_cppflags)
LOCAL_C_INCLUDES := $(libstlport_c_includes)
LOCAL_EXPORT_C_INCLUDES := $(libstlport_c_includes)
LOCAL_CPP_FEATURES := rtti exceptions
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := stlport_shared
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SRC_FILES := $(libstlport_src_files)
LOCAL_SRC_FILES += $(libgabi++_src_files:%=../gabi++/%)
LOCAL_CFLAGS := $(libstlport_cflags)
LOCAL_CPPFLAGS := $(libstlport_cppflags)
LOCAL_C_INCLUDES := $(libstlport_c_includes)
LOCAL_EXPORT_C_INCLUDES := $(libstlport_c_includes)
LOCAL_CPP_FEATURES := rtti exceptions
include $(BUILD_SHARED_LIBRARY)

endif # STLPORT_FORCE_REBUILD == true
